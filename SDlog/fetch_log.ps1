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
  $serial.Open(); Start-Sleep -Milliseconds 500
  $serial.Write("LOG,1,IDENTIFY`n")
  $identity = $null; $deadline = [DateTime]::UtcNow.AddMinutes(3); $recoveringTransfer=$false
  while ([DateTime]::UtcNow -lt $deadline) {
    try { $line = $serial.ReadLine().Trim() } catch [TimeoutException] { continue }
    if ($line -match '^LOG,1,IDENTIFY,([^,]+),([^,]+)$') { Write-Host "DEVICE: $line"; $identity = $line.Split(','); break }
    if ($line -match '^LOG,1,BLOCK,([0-9]+),([0-9]+),([0-9A-Fa-f]+)$') {
      if(-not $recoveringTransfer) { Write-Host 'An interrupted transfer was in progress; draining verified blocks before retrying IDENTIFY.'; $recoveringTransfer=$true }
      $length=[int]::Parse($Matches[2]); $blockCrc=[Convert]::ToUInt32($Matches[3],16)
      if($length -le 0 -or $length -gt 4096){throw "Invalid orphan block length: $line"}
      $block=[byte[]]::new($length); $received=0; $blockDeadline=[DateTime]::UtcNow.AddSeconds(60)
      while($received -lt $length -and [DateTime]::UtcNow -lt $blockDeadline) {
        try { $n=$serial.Read($block,$received,$length-$received); if($n -gt 0){$received+=$n} }
        catch [TimeoutException] { continue }
      }
      if($received -ne $length){throw "Timed out draining interrupted block at offset $($Matches[1])."}
      if([SDlogCrc32]::Compute($block) -eq $blockCrc){$serial.Write('+')}
      else {$serial.Write('-')}
      continue
    }
    if($line -match '^LOG,1,END,[0-9]+,[0-9A-Fa-f]+$') {
      if($recoveringTransfer) { Write-Host 'Interrupted transfer drained; retrying IDENTIFY.'; $recoveringTransfer=$false }
      Start-Sleep -Milliseconds 100; $serial.Write("LOG,1,IDENTIFY`n"); $serial.BaseStream.Flush(); continue
    }
    if($line -match '^(LOG,1,)?ERROR,') {
      if($recoveringTransfer -or $line -match 'ACK_TIMEOUT') {
        Start-Sleep -Milliseconds 100; $serial.Write("LOG,1,IDENTIFY`n"); $serial.BaseStream.Flush(); continue
      }
      throw "Device rejected IDENTIFY: $line"
    } elseif($line) { Write-Host "DEVICE: $line" }
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
        (Test-Path -LiteralPath (Join-Path $dataDir (".$base.$safeName.payload"))) -or
        (Test-Path -LiteralPath (Join-Path $dataDir (".$base.$safeName.csv"))) -or
        (Test-Path -LiteralPath (Join-Path $dataDir ($base+'_steps.csv'))) -or
        (Test-Path -LiteralPath (Join-Path $dataDir ($base+'_metadata.json')))) {
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
      $converter=Join-Path $root ("sys_identification\{0}\convert_multi_cycle.py" -f $firmware)
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
    'TCBIN3' {
      $converter=Join-Path $root 'controller_implementation\trans_ctrl_notch5\convert_trans_log.py'
      & python $converter $payloadPath -o $outputPath
      if($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outputPath)){throw "Controller conversion failed with exit code $LASTEXITCODE"}
    }
    'GRAVPD1' {
      $reader=[IO.File]::OpenText($payloadPath)
      try {
        $metadata=@{}; $expectedHeader='time_us,m1_angle_rad,m1_velocity_rad_s,m1_command_torque_Nm,m1_feedback_torque_Nm,m2_angle_rad,m2_velocity_rad_s,m2_command_torque_Nm,m2_feedback_torque_Nm'
        $headerSeen=$false; $dataEnded=$false; $rowCount=0; $declaredRows=$null
        $numberPattern='^-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$'
        while(($line=$reader.ReadLine()) -ne $null) {
          if($line.StartsWith('#')) {
            if($line -match '^#rows,([0-9]+)$') { $declaredRows=[long]::Parse($Matches[1]); $dataEnded=$true; continue }
            if($line -match '^#([^,]+),(.*)$') { $metadata[$Matches[1]]=$Matches[2] }
            continue
          }
          if($dataEnded) { throw 'GRAVPD1 has data after its completion footer.' }
          if(-not $headerSeen) {
            if($line -cne $expectedHeader) { throw 'GRAVPD1 data header does not match the expected nine columns.' }
            $headerSeen=$true; continue
          }
          $fields=$line.Split(',')
          if($fields.Length -ne 9) { throw "GRAVPD1 row $rowCount does not contain nine fields." }
          foreach($field in $fields) { if($field -cnotmatch $numberPattern) { throw "GRAVPD1 row $rowCount contains a non-numeric field." } }
          $rowCount++
        }
        foreach($key in @('firmware','schema','control_period_us','motor1_id','motor1_can_bus','motor2_id','motor2_can_bus','kp_Nm_per_deg','kd_Nm_per_rad_s','torque_limit_Nm','MIT_kp','MIT_kd','target_angle_m1_rad','target_angle_m2_rad','stop_reason')) {
          if(-not $metadata.ContainsKey($key)) { throw "GRAVPD1 is missing metadata '$key'." }
        }
        if($metadata['firmware'] -cne 'gravity_estimate_PD' -or $metadata['schema'] -cne 'GRAVPD1') { throw 'GRAVPD1 metadata does not match its schema identity.' }
        if(-not $headerSeen -or $null -eq $declaredRows -or $declaredRows -ne $rowCount) { throw 'GRAVPD1 header or completion row count is missing or inconsistent.' }
      } finally { $reader.Dispose() }
      Move-Item -LiteralPath $payloadPath -Destination $outputPath; $payloadPath=$null
    }
    {$_ -in 'CHIRPSD6','CHIRPSI1'} {
      $converter=Join-Path $root 'sys_identification\convert_sd_chirp.py'
      & python $converter $payloadPath
      if($LASTEXITCODE -ne 0){throw "Chirp conversion failed with exit code $LASTEXITCODE"}
      $converted=[IO.Path]::ChangeExtension($payloadPath,'.csv')
      if(-not (Test-Path -LiteralPath $converted)){throw 'Chirp converter did not produce CSV.'}
      Move-Item -LiteralPath $converted -Destination $outputPath
      Remove-Item -LiteralPath ([IO.Path]::ChangeExtension($payloadPath,'.config.json')) -ErrorAction SilentlyContinue
    }
    {$_ -in 'CONSTVL1','STEPTOR1','VELRAMP1'} {
      $converter=Join-Path $root 'SDlog\convert_motion_log.py'
      & python $converter $payloadPath -o $outputPath
      if($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outputPath)){throw "Motion-log conversion failed with exit code $LASTEXITCODE"}
    }
    'FRIC001' {
      $converter=Join-Path $root 'fric_identification\convert_friction.py'
      & python $converter $payloadPath
      if($LASTEXITCODE -ne 0){throw "Friction conversion failed with exit code $LASTEXITCODE"}
      $converted=[IO.Path]::ChangeExtension($payloadPath,'.csv')
      if(-not (Test-Path -LiteralPath $converted)){throw 'Friction converter did not produce CSV.'}
      Move-Item -LiteralPath $converted -Destination $outputPath
      $steps=[IO.Path]::ChangeExtension($payloadPath,'.steps.csv')
      if(Test-Path -LiteralPath $steps){Move-Item -LiteralPath $steps -Destination (Join-Path $dataDir ($base+'_steps.csv'))}
      $metadata=[IO.Path]::ChangeExtension($payloadPath,'.metadata.json')
      if(Test-Path -LiteralPath $metadata){Move-Item -LiteralPath $metadata -Destination (Join-Path $dataDir ($base+'_metadata.json'))}
    }
    {$_ -in 'CSV-RAW','CSV-FRIC1'} {
      $reader=[IO.File]::OpenText($payloadPath)
      try {
        $firstLine=$reader.ReadLine(); $hasComma=$false; $hasFooter=$false
        if($firstLine -and $firstLine.Contains(',')){$hasComma=$true}
        while(($line=$reader.ReadLine()) -ne $null) {
          if($line.Contains(',')){$hasComma=$true}
          if($line -match '^# result=' -or $line -match '^#rows,'){$hasFooter=$true}
        }
      } finally { $reader.Dispose() }
      if(-not $hasComma -or -not $hasFooter){throw "CSV payload failed header/footer validation; CRC-verified payload retained at $payloadPath"}
      Move-Item -LiteralPath $payloadPath -Destination $outputPath; $payloadPath=$null
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
