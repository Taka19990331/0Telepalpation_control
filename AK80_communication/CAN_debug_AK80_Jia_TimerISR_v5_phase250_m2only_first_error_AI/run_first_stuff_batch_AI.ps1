param(
  [string]$PortName = 'COM3',
  [int]$Trials = 5,
  [int]$DurationSeconds = 10
)

$ErrorActionPreference = 'Stop'
if ($Trials -lt 1 -or $Trials -gt 5) { throw 'Trials must be 1..5' }
if ($DurationSeconds -ne 10) { throw 'Chat-approved first-STUFF runs are 10 seconds max' }

$rootPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$logDir = Join-Path $rootPath 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$device = [System.IO.Ports.SerialPort]::new(
  $PortName, 115200, [System.IO.Ports.Parity]::None, 8,
  [System.IO.Ports.StopBits]::One
)
$device.Handshake = [System.IO.Ports.Handshake]::None
$device.DtrEnable = $false
$device.RtsEnable = $false
$device.ReadTimeout = 200
$device.WriteTimeout = 1000

function Send-AllowedCommand([System.IO.Ports.SerialPort]$serial, [System.IO.StreamWriter]$writer, [string]$command, [string]$tag) {
  if ($command -notin @('STATUS', 'S', 'X', 'EVENTS', 'HISTORY')) { throw "Not allowlisted: $command" }
  $writer.WriteLine("# $tag HOST_COMMAND=$command")
  $serial.WriteLine($command)
}

function Read-Window([System.IO.Ports.SerialPort]$serial, [System.IO.StreamWriter]$writer, [string]$tag, [double]$maxSeconds, [double]$quietSeconds = 0) {
  $lines = [System.Collections.Generic.List[string]]::new()
  $clock = [System.Diagnostics.Stopwatch]::StartNew()
  $lastLineAt = 0.0
  while ($clock.Elapsed.TotalSeconds -lt $maxSeconds) {
    try {
      $line = $serial.ReadLine().TrimEnd("`r", "`n")
      if ($line) {
        $lines.Add($line)
        $writer.WriteLine(("{0:F6},{1},{2}" -f $clock.Elapsed.TotalSeconds, $tag, $line))
        $lastLineAt = $clock.Elapsed.TotalSeconds
      }
    } catch [TimeoutException] {
      if ($quietSeconds -gt 0 -and $lines.Count -gt 0 -and ($clock.Elapsed.TotalSeconds - $lastLineAt) -ge $quietSeconds) { break }
    }
  }
  $clock.Stop()
  return ,$lines.ToArray()
}

try {
  $device.Open()
  for ($trial = 1; $trial -le $Trials; $trial++) {
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $logPath = Join-Path $logDir ("first_stuff_trial{0}_{1}.log" -f $trial, $stamp)
    if (Test-Path -LiteralPath $logPath) { throw "Refusing to overwrite $logPath" }
    $writer = [System.IO.StreamWriter]::new($logPath, $false, [System.Text.UTF8Encoding]::new($false))
    $writer.AutoFlush = $true
    try {
      $writer.WriteLine("# TEST=V5_CAN2_FIRST_ERROR_DIAGNOSIS; TRIAL=$trial; PORT=$PortName; PERIOD_US=500; EXPECTED=M2_ONLY; ZERO_TORQUE_ONLY")
      Send-AllowedCommand $device $writer 'STATUS' 'PREFLIGHT'
      $statusLines = Read-Window $device $writer 'PREFLIGHT' 3.0
      $statusOk = $false
      foreach ($line in $statusLines) {
        if ($line.StartsWith('STATE,IDLE,PERIOD_US,500,RX_POLL_US,100,')) { $statusOk = $true }
      }
      $m2Seen = @($statusLines | Where-Object { $_.StartsWith('M2 TX:') }).Count -gt 0
      $m1Seen = @($statusLines | Where-Object { $_.StartsWith('M1 TX:') }).Count -gt 0
      if (-not $statusOk -or -not $m2Seen -or $m1Seen) {
        $writer.WriteLine("# PREFLIGHT_ABORT=STATE_MOTOR_OR_PERIOD_MISMATCH; S_NOT_SENT=1")
        Write-Host "trial=$trial preflight mismatch; S was not sent; log=$logPath"
        break
      }

      Send-AllowedCommand $device $writer 'S' 'RUN'
      $runClock = [System.Diagnostics.Stopwatch]::StartNew()
      $stopReason = 'duration'
      $hostStopSent = $false
      while ($runClock.Elapsed.TotalSeconds -lt $DurationSeconds) {
        try {
          $line = $device.ReadLine().TrimEnd("`r", "`n")
          if (-not $line) { continue }
          $writer.WriteLine(("{0:F6},RUN,{1}" -f $runClock.Elapsed.TotalSeconds, $line))

          if ($line.Contains('ERROR CAN_STUFF')) {
            $stopReason = 'first_stuff_firmware_auto_stop'
            Send-AllowedCommand $device $writer 'X' 'NORMALIZE'
            $hostStopSent = $true
            break
          }
          if ($line.Contains('CAN_IRQ_ERROR_SNAPSHOT') -and $line.Contains('STUFF')) {
            # Firmware latches the stop on this exact first-STUFF snapshot.
            # Wait for ERROR CAN_STUFF before sending X, avoiding a race.
            continue
          }
          if ($line.Contains('FAULT REASON=CAN_STUFF')) {
            # The detailed snapshot is printed immediately after this event.
            continue
          }
          if ($line.Contains('CAN_IRQ_ERROR_SNAPSHOT') -or $line.Contains(' CAN_ERROR ') -or
              $line.Contains('FAULT REASON=') -or $line.StartsWith('ERROR ') -or
              $line.StartsWith('START_ERROR')) {
            $stopReason = 'other_communication_or_firmware_fault'
            Send-AllowedCommand $device $writer 'X' 'FAULT_STOP'
            $hostStopSent = $true
            break
          }
        } catch [TimeoutException] {}
      }
      $runClock.Stop()
      if (-not $hostStopSent) {
        Send-AllowedCommand $device $writer 'X' 'DURATION_STOP'
        $hostStopSent = $true
      }
      $writer.WriteLine("# RESULT=trial:$trial; elapsed_s:$([Math]::Round($runClock.Elapsed.TotalSeconds,3)); stop_reason:$stopReason; host_X_sent:$([int]$hostStopSent)")

      # Drain stop/fault output, then require IDLE before read-only history.
      [void](Read-Window $device $writer 'STOP_OUTPUT' 3.0 0.7)
      Send-AllowedCommand $device $writer 'STATUS' 'POST_STOP'
      $postStatus = Read-Window $device $writer 'POST_STOP' 3.0
      $isIdle = @($postStatus | Where-Object { $_.StartsWith('STATE,IDLE,') }).Count -gt 0
      if (-not $isIdle) {
        $writer.WriteLine('# POST_STOP_ABORT=IDLE_NOT_CONFIRMED; EVENTS_HISTORY_NOT_QUERIED; BATCH_STOPPED=1')
        Write-Host "trial=$trial IDLE not confirmed; stopping batch; log=$logPath"
        break
      }
      Send-AllowedCommand $device $writer 'EVENTS' 'POST_STOP'
      [void](Read-Window $device $writer 'EVENTS' 5.0 1.0)
      Send-AllowedCommand $device $writer 'HISTORY' 'POST_STOP'
      [void](Read-Window $device $writer 'HISTORY' 8.0 1.2)
      $writer.WriteLine("# TRIAL_COMPLETE=$trial; STOP_REASON=$stopReason; IDLE_CONFIRMED=1")
      Write-Host "trial=$trial complete; stop_reason=$stopReason; log=$logPath"
    } finally {
      $writer.Dispose()
    }
  }
} finally {
  if ($device.IsOpen) { $device.Close() }
  $device.Dispose()
}
