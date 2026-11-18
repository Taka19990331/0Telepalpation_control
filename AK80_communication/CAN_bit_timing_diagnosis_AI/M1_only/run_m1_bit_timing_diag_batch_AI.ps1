param(
  [string]$PortName = 'COM3',
  [int]$Trials = 2,
  [int]$DurationSeconds = 20
)

$ErrorActionPreference = 'Stop'
if ($Trials -lt 1 -or $Trials -gt 2) { throw 'Trials must be 1..2' }
if ($DurationSeconds -ne 20) { throw 'Chat-approved bit timing runs are capped at 20 seconds' }

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
    $logPath = Join-Path $logDir ("m1_bit_timing_trial{0}_{1}.log" -f $trial, $stamp)
    if (Test-Path -LiteralPath $logPath) { throw "Refusing to overwrite $logPath" }
    $writer = [System.IO.StreamWriter]::new($logPath, $false, [System.Text.UTF8Encoding]::new($false))
    $writer.AutoFlush = $true
    $runStarted = $false
    try {
      $writer.WriteLine("# TEST=CAN_BIT_TIMING_DIAGNOSIS; TRIAL=$trial; PORT=$PortName; PERIOD_US=500; DURATION_CAP_S=20; EXPECTED=M1_ONLY; ZERO_TORQUE_ONLY")
      Send-AllowedCommand $device $writer 'STATUS' 'PREFLIGHT'
      $statusLines = Read-Window $device $writer 'PREFLIGHT' 3.0
      $statusOk = $false
      foreach ($line in $statusLines) {
        if ($line.StartsWith('STATE,IDLE,PERIOD_US,500,RX_POLL_US,100,')) { $statusOk = $true }
      }
      $m1Seen = @($statusLines | Where-Object { $_.StartsWith('M1 TX:') }).Count -gt 0
      $m2Seen = @($statusLines | Where-Object { $_.StartsWith('M2 TX:') }).Count -gt 0
      if (-not $statusOk -or -not $m1Seen -or $m2Seen) {
        $writer.WriteLine("# PREFLIGHT_ABORT=STATE_MOTOR_OR_PERIOD_MISMATCH; S_NOT_SENT=1")
        Write-Host "trial=$trial preflight mismatch; S was not sent; log=$logPath"
        break
      }

      $runStarted = $true
      Send-AllowedCommand $device $writer 'S' 'RUN'
      $runClock = [System.Diagnostics.Stopwatch]::StartNew()
      $stopReason = 'duration'
      $hostStopSent = $false
      while ($runClock.Elapsed.TotalSeconds -lt $DurationSeconds) {
        try {
          $line = $device.ReadLine().TrimEnd("`r", "`n")
          if (-not $line) { continue }
          $writer.WriteLine(("{0:F6},RUN,{1}" -f $runClock.Elapsed.TotalSeconds, $line))

          if ($line.Contains('ERROR CAN_ERROR')) {
            $stopReason = 'first_can_error_firmware_auto_stop'
            Send-AllowedCommand $device $writer 'X' 'NORMALIZE'
            $hostStopSent = $true
            $runStarted = $false
            break
          }
          if ($line.Contains('CAN_IRQ_ERROR_SNAPSHOT') -or $line.Contains('FAULT REASON=')) {
            # Wait for ERROR CAN_ERROR (with the full first-snapshot report)
            # before normalizing the latched stop to IDLE.
            continue
          }
          if ($line.StartsWith('ERROR ') -or $line.StartsWith('START_ERROR')) {
            $stopReason = 'other_communication_or_firmware_fault'
            Send-AllowedCommand $device $writer 'X' 'FAULT_STOP'
            $hostStopSent = $true
            $runStarted = $false
            break
          }
        } catch [TimeoutException] {}
        catch {
          $stopReason = 'host_serial_exception'
          $writer.WriteLine("# SERIAL_EXCEPTION=$($_.Exception.Message)")
          try {
            Send-AllowedCommand $device $writer 'X' 'SERIAL_EXCEPTION_STOP'
            $hostStopSent = $true
            $runStarted = $false
          } catch {
            $writer.WriteLine("# SERIAL_X_FAILED=$($_.Exception.Message)")
          }
          break
        }
      }
      $runClock.Stop()
      if (-not $hostStopSent) {
        Send-AllowedCommand $device $writer 'X' 'DURATION_STOP_NO_ERROR'
        if ($stopReason -eq 'duration') { $stopReason = '20s_no_can_error' }
        $hostStopSent = $true
        $runStarted = $false
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
      if ($runStarted -and $device.IsOpen) {
        try {
          $writer.WriteLine('# FINALLY_BEST_EFFORT_X=1')
          $device.WriteLine('X')
        } catch {}
      }
      $writer.Dispose()
    }
  }
} finally {
  if ($device.IsOpen) { $device.Close() }
  $device.Dispose()
}
