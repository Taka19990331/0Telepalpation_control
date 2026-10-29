param(
  [Parameter(Mandatory=$true)][string]$Port,
  [string]$OutputDirectory = (Join-Path $PSScriptRoot 'data')
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$serial = [System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.ReadTimeout=1000; $serial.WriteTimeout=5000; $serial.NewLine="`n"
try {
  $serial.Open(); Start-Sleep -Seconds 2; $serial.DiscardInBuffer()
  $serial.Write('d')
  $lines=[System.Collections.Generic.List[string]]::new(); $receiving=$false; $done=$false; $name=''; $rows=-1
  $deadline=[DateTime]::UtcNow.AddMinutes(5)
  while([DateTime]::UtcNow -lt $deadline){
    try{$line=$serial.ReadLine().TrimEnd("`r")}catch [System.TimeoutException]{continue}
    if(-not $receiving){if($line.StartsWith('TRANS_BEGIN,')){$p=$line.Split(','); if($p.Length -ne 3 -or $p[1] -notmatch '^TC\d{4}\.BIN$'){throw "Bad transfer header: $line"};$name=$p[1];$rows=[int]$p[2];$receiving=$true};continue}
    if($line.StartsWith('TRANS_END,')){$p=$line.Split(',');if($p.Length -ne 3 -or $p[1] -ne $name -or [int]$p[2] -ne $rows){throw "Transfer end mismatch: $line"};$done=$true;break}
    $lines.Add($line)
  }
  if(-not $done){throw 'Timed out waiting for SD transfer.'}
  $header='time_us,command_torque_nm';$i=$lines.IndexOf($header);if($i -lt 0){throw 'CSV header missing.'}
  if(($lines.Count-$i-1)-ne $rows){throw 'CSV row count mismatch.'}
  $dest=Join-Path $OutputDirectory ('trans_ctrl_test1_'+[DateTime]::Now.ToString('yyyyMMdd_HHmmss')+'.csv')
  [IO.File]::WriteAllLines($dest,$lines.ToArray(),[Text.UTF8Encoding]::new($false))
  Write-Host "Saved $rows rows from $name to $dest"
} finally {if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
