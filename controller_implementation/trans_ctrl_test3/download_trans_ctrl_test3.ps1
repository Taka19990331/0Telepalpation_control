param(
  [Parameter(Mandatory=$true)][string]$Port,
  [string]$OutputDirectory = (Join-Path $PSScriptRoot 'data')
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$serial = [System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.ReadTimeout=1000; $serial.WriteTimeout=5000; $serial.NewLine="`n"
# USB CDC can deliver bursts faster than PowerShell consumes individual lines.
$serial.ReadBufferSize=16777216
try {
  $serial.Open(); Start-Sleep -Seconds 2; $serial.DiscardInBuffer()
  $serial.Write("d`n")
  $lines=[System.Collections.Generic.List[string]]::new(); $receiving=$false; $done=$false; $name=''; $rows=-1; $chunks=0
  $raw=[Text.StringBuilder]::new()
  $deadline=[DateTime]::UtcNow.AddMinutes(10)
  while([DateTime]::UtcNow -lt $deadline){
    try{$line=$serial.ReadLine().TrimEnd("`r")}catch [System.TimeoutException]{continue}
    [void]$raw.Append($line).Append("`n")
    if(-not $receiving){if($line.StartsWith('TRANS_BEGIN,')){$p=$line.Split(','); if($p.Length -ne 3 -or $p[1] -notmatch '^TC\d{4}\.CSV$'){throw "Bad transfer header: $line"};$name=$p[1];$rows=[int]$p[2];$receiving=$true};continue}
    if($line.StartsWith('TRANS_END,')){$p=$line.Split(',');if($p.Length -ne 3 -or $p[1] -ne $name -or [int]$p[2] -ne $rows){throw "Transfer end mismatch: $line"};$done=$true;break}
    if($line.StartsWith('TRANS_ABORT,')){throw "Device aborted transfer: $line"}
    if($line.StartsWith('TRANS_CHUNK,')){$p=$line.Split(',');if($p.Length -ne 2 -or [int]$p[1] -ne $chunks){throw "Unexpected transfer chunk: $line"};$serial.Write('A');++$chunks;continue}
    $lines.Add($line)
  }
  if(-not $done){throw 'Timed out waiting for SD transfer.'}
  $header='timestamp_us,position_rad,velocity_raw_rad_s,velocity_filtered_rad_s,feedback_torque_Nm,torque_bias_Nm,feedback_age_us,friction_model_Nm,rfob_input_Nm,rfob_filter_state_Nm,external_torque_est_Nm,torque_RFOB_Nm,torque_FF_Nm,torque_cmd_unsat_Nm,torque_cmd_Nm,K_R,K_F,control_enabled,torque_saturated,motor_error,fault_code';$i=-1; for($j=0;$j -lt $lines.Count;++$j){if($lines[$j] -eq $header -or $lines[$j].StartsWith($header+',')){$i=$j;break}};if($i -lt 0){throw 'CSV header missing.'}
  $receivedRows=@($lines | Where-Object { $_ -match '^\d+,' }).Count
  if($receivedRows-ne $rows){
    $partial=Join-Path $OutputDirectory ('transfer_incomplete_'+[DateTime]::Now.ToString('yyyyMMdd_HHmmss')+'.txt')
    [IO.File]::WriteAllText($partial,$raw.ToString(),[Text.UTF8Encoding]::new($false))
    throw "CSV row count mismatch: expected $rows, received $receivedRows. Diagnostic capture: $partial"
  }
  $badRows=@($lines | Where-Object { $_ -match '^\d+,' -and $_.Split(',').Count -ne 29 })
  if($badRows.Count){throw "CSV row width mismatch: $($badRows.Count) data rows do not have 29 fields."}
  $dest=Join-Path $OutputDirectory ('trans_ctrl_test3_'+[DateTime]::Now.ToString('yyyyMMdd_HHmmss')+'.csv')
  [IO.File]::WriteAllLines($dest,$lines.ToArray(),[Text.UTF8Encoding]::new($false))
  Write-Host "Saved $rows rows from $name to $dest"
} finally {if($serial.IsOpen){$serial.Close()};$serial.Dispose()}



