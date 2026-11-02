param(
  [string]$Port='',
  [int]$Trials=3,
  [string]$Label='hardware',
  [string]$OutDir=(Join-Path $PSScriptRoot 'data'),
  [int]$ReconnectTimeoutSec=20
)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force $OutDir | Out-Null
$logPath=Join-Path $OutDir ($Label+'_serial.log')
$csvPath=Join-Path $OutDir ($Label+'_trials.csv')
$watch=[Diagnostics.Stopwatch]::StartNew()
$log=[System.Text.StringBuilder]::new()
$segment=[System.Text.StringBuilder]::new()
$teensyPorts='C:\Users\Miyam\AppData\Local\Arduino15\packages\teensy\tools\teensy-tools\1.62.0\teensy_ports.exe'
function Find-TeensyPort {
  if(-not (Test-Path -LiteralPath $teensyPorts)){return $null}
  $listing=& $teensyPorts -L 2>$null
  foreach($line in $listing) {
    if($line -match 'COM\d+\s+\(Teensy 4\.1\)\s+Serial') {
      if($line -match '(COM\d+)') { return $Matches[1] }
    }
  }
  return $null
}
function Read-Serial {
  Start-Sleep -Milliseconds 10
  $chunk=$serial.ReadExisting()
  if($chunk){[void]$log.Append($chunk); [void]$segment.Append($chunk)}
  return $chunk
}
function Has-Marker([string]$text,[string]$pattern) { return [regex]::IsMatch($text,$pattern) }
try {
  $serial=$null; $lastOpenError=''; $connectDeadline=[DateTime]::UtcNow.AddSeconds($ReconnectTimeoutSec)
  while(-not $serial -and [DateTime]::UtcNow -lt $connectDeadline) {
    $candidate=if($Port){$Port}else{Find-TeensyPort}
    if($candidate) {
      $candidateSerial=[System.IO.Ports.SerialPort]::new($candidate,115200,'None',8,'One')
      $candidateSerial.ReadTimeout=100; $candidateSerial.WriteTimeout=1000
      $candidateSerial.DtrEnable=$true; $candidateSerial.RtsEnable=$false
      try { $candidateSerial.Open(); $serial=$candidateSerial; $Port=$candidate }
      catch { $lastOpenError=$_.Exception.Message; $candidateSerial.Dispose() }
    }
    if(-not $serial){Start-Sleep -Milliseconds 250}
  }
  if(-not $serial) { throw ('Teensy USB serial reconnect timed out after '+$ReconnectTimeoutSec+'s. Last port='+$Port+'; error='+$lastOpenError+'. Check cable/board and close any serial monitor using the port.') }
  for($i=0;$i -lt 30;$i++){[void](Read-Serial)}
  $bootSegment=$log.ToString()
  $rows=[System.Collections.Generic.List[object]]::new()
  for($trial=1;$trial -le $Trials;$trial++) {
    $segment.Clear() | Out-Null
    $begin=$watch.Elapsed.TotalMilliseconds
    $serial.WriteLine('S')
    $serial.WriteLine('ON')
    $replyDeadline=$watch.Elapsed.TotalSeconds+3
    while($watch.Elapsed.TotalSeconds -lt $replyDeadline -and
          -not (Has-Marker $segment.ToString() 'START_ACCEPT|START_REJECT|START_FAIL')) { [void](Read-Serial) }
    $reply=$watch.Elapsed.TotalMilliseconds
    $seg=$segment.ToString()
    $accepted=$seg -match 'START_ACCEPT'
    $saved=$false; $savedAt=$null
    $deadline=$watch.Elapsed.TotalSeconds+3
    while($watch.Elapsed.TotalSeconds -lt $deadline -and -not $saved) {
      [void](Read-Serial)
      $saved=Has-Marker $segment.ToString() 'SAVED,'
      if($saved){$savedAt=$watch.Elapsed.TotalMilliseconds}
    }
    $serial.WriteLine('STATUS')
    for($n=0;$n -lt 20;$n++){[void](Read-Serial)}
    $seg=$segment.ToString()
    $reject=([regex]::Matches($seg,'START_REJECT,[^\r\n]+|START_FAIL,[^\r\n]+') | Select-Object -First 1).Value
    $saveLine=([regex]::Matches($seg,'SAVED,[^\r\n]+') | Select-Object -Last 1).Value
    $finalizeUs=''; $finalizeSyncUs=''
    $finalizeMatch=[regex]::Match($saveLine,'FINALIZE_US,(\d+),SYNC_US,(\d+)')
    if($finalizeMatch.Success){$finalizeUs=$finalizeMatch.Groups[1].Value;$finalizeSyncUs=$finalizeMatch.Groups[2].Value}
    $status=([regex]::Matches($seg,'STATUS,ON,[^\r\n]+') | Select-Object -Last 1).Value
    $reason=([regex]::Matches($seg,'STOP,reason,\d+') | Select-Object -Last 1).Value
    $phase=([regex]::Matches($seg,'START_PHASE,search_us,[^\r\n]+') | Select-Object -First 1).Value
    $active=([regex]::Matches($seg,'START_ACTIVE,elapsed_us,\d+') | Select-Object -First 1).Value
    $boot=([regex]::Matches($bootSegment,'BOOT_TIMING,[^\r\n]+') | Select-Object -First 1).Value
    $bootSd=([regex]::Matches($bootSegment,'BOOT_SD,[^\r\n]+') | Select-Object -First 1).Value
    $onAccepted=Has-Marker $seg 'ON_ACCEPT,enabled,1'
    $tx=''; $sdFailures=''; $fault=''
    $faultMatch=[regex]::Match($status,',FAULT,(\d+)'); if($faultMatch.Success){$fault=$faultMatch.Groups[1].Value}
    $diagMatch=[regex]::Match($seg,'(?s)#diag,write_success,(\d+).*?#diag,sd_write_failures,(\d+)')
    if($diagMatch.Success){$tx=$diagMatch.Groups[1].Value;$sdFailures=$diagMatch.Groups[2].Value}
    $savedMs=if($saved){[math]::Round($savedAt-$begin,3)}else{''}
    $rows.Add([pscustomobject]@{
      label=$Label;trial=$trial;s_to_start_reply_ms=[math]::Round($reply-$begin,3)
      s_to_saved_ms=$savedMs;start_accepted=$accepted;on_accepted=$onAccepted;reject_reason=$reject;saved=$saved
      active_timing=$active;start_sd_phases=$phase;stop_reason=$reason;fault_code=$fault
      can_write_success=$tx;sd_write_failures=$sdFailures;finalize_us=$finalizeUs;finalize_sync_us=$finalizeSyncUs
      boot_timing=$boot;boot_sd=$bootSd;saved_line=$saveLine
    })
    [void]$log.AppendLine("TRIAL_RESULT,$trial,start_accepted,$accepted,saved,$saved,s_to_saved_ms,$savedMs")
    Start-Sleep -Milliseconds 100
  }
  $segment.Clear() | Out-Null
  $serial.WriteLine('D')
  $ackSent=$false; $dumpDone=$false; $dumpDeadline=$watch.Elapsed.TotalSeconds+8
  while($watch.Elapsed.TotalSeconds -lt $dumpDeadline -and -not $dumpDone) {
    [void](Read-Serial)
    $dumpText=$segment.ToString()
    if(-not $ackSent -and (Has-Marker $dumpText 'TRANS_CHUNK,0')) { $serial.Write('A'); $ackSent=$true }
    if(Has-Marker $dumpText 'TRANS_END,') { $dumpDone=$true }
  }
  [void]$log.AppendLine("SD_DUMP_VALIDATED,$dumpDone,ACK_SENT,$ackSent")
  foreach($row in $rows) { $row | Add-Member -NotePropertyName sd_dump_verified -NotePropertyValue $dumpDone }
  [IO.File]::WriteAllText($logPath,$log.ToString())
  $rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding utf8
  $rows | Format-Table -AutoSize | Out-String | Write-Output
  "LOG=$logPath"; "CSV=$csvPath"
} finally { if($serial -and $serial.IsOpen){$serial.Close()} }
