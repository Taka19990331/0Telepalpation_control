param(
  [string]$Port = 'COM3',
  [ValidateSet('LATEST','PREVIOUS')][string]$Which = 'LATEST'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dataDir = Join-Path $root 'data'
[IO.Directory]::CreateDirectory($dataDir) | Out-Null

Add-Type -TypeDefinition @'
using System;
public static class SDlogCrc32 {
  private static readonly uint[] Table = Build();
  private static uint[] Build() {
    var t = new uint[256];
    for (uint i=0;i<256;i++) { uint c=i; for(int b=0;b<8;b++) c=(c&1)!=0?0xedb88320u^(c>>1):c>>1; t[i]=c; }
    return t;
  }
  public static uint Update(uint crc, byte[] data, int count) {
    for(int i=0;i<count;i++) crc=Table[(crc^data[i])&255]^(crc>>8);
    return crc;
  }
  public static uint Compute(byte[] data) { return Update(0xffffffffu,data,data.Length)^0xffffffffu; }
}
'@

$serial = $null; $tempPath = $null; $payloadPath = $null
try {
  $serial = [IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
  $serial.ReadBufferSize = 4194304
  $serial.DtrEnable = $false; $serial.RtsEnable = $false
  $serial.ReadTimeout = 1000; $serial.WriteTimeout = 10000; $serial.NewLine = "`n"
  $serial.Open(); Start-Sleep -Milliseconds 500; $serial.DiscardInBuffer()
  $serial.Write("LOG,1,IDENTIFY`n")
  $identity = $null; $deadline = [DateTime]::UtcNow.AddSeconds(15)
  while ([DateTime]::UtcNow -lt $deadline) {
    try { $line = $serial.ReadLine().Trim() } catch [TimeoutException] { continue }
    if ($line) { Write-Host "DEVICE: $line" }
    if ($line -match '^LOG,1,IDENTIFY,([^,]+),([^,]+)$') { $identity = $line.Split(','); break }
    if ($line -match '^(LOG,1,)?ERROR,') { throw "Device: $line" }
  }
  if (-not $identity) { throw 'No common logging identity response. Ensure a _LOGtest firmware is running.' }
  $firmware = $identity[3]; $schema = $identity[4]
  Start-Sleep -Milliseconds 100
  $serial.WriteLine("LOG,1,FETCH,$Which")
  $serial.BaseStream.Flush()

  # SD file discovery/validation and the first whole-file CRC pass may take
  # appreciable time on large logs before the device can announce metadata.
  $fileHeader = $null; $deadline = [DateTime]::UtcNow.AddMinutes(2)
  while ([DateTime]::UtcNow -lt $deadline) {
    try { $line = $serial.ReadLine().Trim() } catch [TimeoutException] { continue }
    if ($line) { Write-Host "DEVICE: $line" }
    if ($line -match '^(LOG,1,)?ERROR,') { throw "Device: $line" }
    if ($line.StartsWith('LOG,1,FILE,')) { $fileHeader = $line; break }
  }
  if (-not $fileHeader) { throw 'Timed out waiting for the file header.' }
  $p = $fileHeader.Split(',')
  if ($p.Length -ne 9) { throw "Malformed file header: $fileHeader" }
  if ($p[3] -ne $firmware) { throw "Firmware identity changed during transfer: $($p[3])" }
  $remoteName = $p[5]; $expectedSize = [long]::Parse($p[6]); $expectedCrc = [Convert]::ToUInt32($p[7],16)
  $epoch = [long]::Parse($p[8])
  if ($expectedSize -lt 0) { throw "Invalid payload length: $expectedSize" }
  if ($epoch -gt 0) { $stamp = [DateTimeOffset]::FromUnixTimeSeconds($epoch).ToLocalTime().ToString('yyyyMMdd_HHmmss') }
  else { $stamp = Get-Date -Format 'yyyyMMdd_HHmmss' }
  $safeName = [IO.Path]::GetFileName($remoteName)
  $base = "${firmware}_${stamp}"
  $outputPath = Join-Path $dataDir ($base+'.csv')
  $index=1
  while((Test-Path -LiteralPath $outputPath) -or
        (Test-Path -LiteralPath (Join-Path $dataDir (".$base.$safeName.partial"))) -or
        (Test-Path -LiteralPath (Join-Path $dataDir (".$base.$safeName.payload")))) {
    $base = "{0}_{1:D2}" -f "${firmware}_${stamp}",$index
    $outputPath = Join-Path $dataDir ($base+'.csv')
    $index++
  }
  $tempPath = Join-Path $dataDir (".$base.$safeName.partial")
  $payloadPath = Join-Path $dataDir (".$base.$safeName.payload")
  $stream = [IO.File]::Open($tempPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
  $crc = [uint32]::MaxValue; $offset = 0L
  try {
    while ($offset -lt $expectedSize) {
      $blockHeader = $null; $deadline = [DateTime]::UtcNow.AddSeconds(60)
      while ([DateTime]::UtcNow -lt $deadline) {
        try { $candidate = $serial.ReadLine().Trim() } catch [TimeoutException] { continue }
        if ($candidate -match '^(LOG,1,)?ERROR,') { throw "Device: $candidate" }
        if ($candidate.StartsWith('LOG,1,BLOCK,')) { $blockHeader = $candidate; break }
      }
      if (-not $blockHeader) { throw "Timed out waiting for block at byte $offset." }
      $b = $blockHeader.Split(',')
      if ($b.Length -ne 6) { throw "Malformed block header: $blockHeader" }
      $blockOffset = [long]::Parse($b[3]); $blockLength = [int]::Parse($b[4]); $blockCrc = [Convert]::ToUInt32($b[5],16)
      if ($blockOffset -ne $offset -or $blockLength -le 0 -or $blockLength -gt 4096 -or ($offset+$blockLength) -gt $expectedSize) { throw "Unexpected block header: $blockHeader" }
      $received = 0; $block = [byte[]]::new($blockLength)
      while ($received -lt $blockLength) {
        try { $n=$serial.Read($block,$received,$blockLength-$received); if($n -gt 0){$received+=$n} }
        catch [TimeoutException] {
          # Drain the remainder of this advertised block before asking for retry.
          $serial.ReadTimeout=1000
          while($received -lt $blockLength) { try{$n=$serial.Read($block,$received,$blockLength-$received);if($n -gt 0){$received+=$n}}catch [TimeoutException]{break} }
          break
        }
      }
      $valid = $received -eq $blockLength -and [SDlogCrc32]::Compute($block) -eq $blockCrc
      if (-not $valid) { $serial.Write('-'); continue }
      $stream.Write($block,0,$blockLength)
      $crc = [SDlogCrc32]::Update($crc,$block,$blockLength)
      $serial.Write('+'); $offset += $blockLength
    }
    $stream.Flush($true)
  } finally { $stream.Dispose() }
  if ($offset -ne $expectedSize -or (($crc -bxor [uint32]::MaxValue) -ne $expectedCrc)) { throw 'Whole-file length or CRC32 mismatch.' }
  $endDeadline=[DateTime]::UtcNow.AddSeconds(30); $end=$false
  while([DateTime]::UtcNow -lt $endDeadline) {
    try{$line=$serial.ReadLine().Trim()}catch [TimeoutException]{continue}
    if($line -match '^LOG,1,END,([0-9]+),([0-9A-Fa-f]+)$') { if([long]$Matches[1] -ne $expectedSize -or [Convert]::ToUInt32($Matches[2],16) -ne $expectedCrc){throw "End marker mismatch: $line"};$end=$true;break }
    if($line -match '^(LOG,1,)?ERROR,'){throw "Device: $line"}
  }
  if(-not $end){throw 'Missing verified end marker.'}
  Move-Item -LiteralPath $tempPath -Destination $payloadPath; $tempPath=$null

  switch ($schema) {
    'CHIRPMC1' {
      $converter=Join-Path $root 'sys_identification\chirp_multi-cycle_LOGtest\convert_multi_cycle.py'
      & python $converter $payloadPath
      if($LASTEXITCODE -ne 0){throw "Chirp conversion failed with exit code $LASTEXITCODE"}
      $converted=[IO.Path]::ChangeExtension($payloadPath,'.csv')
      if(-not (Test-Path -LiteralPath $converted)){throw 'Chirp converter did not produce CSV.'}
      Move-Item -LiteralPath $converted -Destination $outputPath
      Remove-Item -LiteralPath ([IO.Path]::ChangeExtension($payloadPath,'.config.json')) -ErrorAction SilentlyContinue
      Remove-Item -LiteralPath ([IO.Path]::ChangeExtension($payloadPath,'.validation.json')) -ErrorAction SilentlyContinue
    }
    'TCBIN2' {
      $converter=Join-Path $root 'controller_implementation\trans_ctrl_test3_LOGtest\convert_trans_log.py'
      & python $converter $payloadPath -o $outputPath
      if($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outputPath)){throw "Controller conversion failed with exit code $LASTEXITCODE"}
    }
    'CSV-FRIC2' {
      $reader=[IO.File]::OpenText($payloadPath)
      try {
        $firstLine=$reader.ReadLine(); $hasHeader=$false; $hasFooter=$false
        while(($line=$reader.ReadLine()) -ne $null) {
          if($line -match '^record_type,time_us,'){$hasHeader=$true}
          if($line -match '^# result='){$hasFooter=$true}
        }
      } finally { $reader.Dispose() }
      if(-not $hasHeader -or -not $hasFooter){throw "Friction CSV is missing its record_type header or completion footer. CRC-verified payload retained at $payloadPath"}
      Move-Item -LiteralPath $payloadPath -Destination $outputPath; $payloadPath=$null
    }
    default { throw "Unsupported schema '$schema'. Verified payload retained at $payloadPath" }
  }
  if($payloadPath) { Remove-Item -LiteralPath $payloadPath -ErrorAction SilentlyContinue; $payloadPath=$null }
  Write-Output "Saved $outputPath ($expectedSize bytes, CRC32 $($expectedCrc.ToString('X8')), schema $schema)."
} catch {
  Write-Error $_.Exception.Message
  exit 1
} finally {
  if($tempPath -and (Test-Path -LiteralPath $tempPath)){Remove-Item -LiteralPath $tempPath -ErrorAction SilentlyContinue}
  if($serial){if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
}
