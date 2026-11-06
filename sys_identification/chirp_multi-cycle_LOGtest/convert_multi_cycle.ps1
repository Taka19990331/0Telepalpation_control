param(
  [Parameter(Mandatory=$true, Position=0, ValueFromRemainingArguments=$true)]
  [string[]] $BinaryFiles
)

$ErrorActionPreference = 'Stop'
$Invariant = [Globalization.CultureInfo]::InvariantCulture
$EncodingAscii = [Text.Encoding]::ASCII
$Columns = 'index,nominal_us,actual_us,period_us,phase,position_deg,speed_rad_s,feedback_torque_estimate_Nm,chirp_Nm,pd_Nm,command_Nm,torque_limited,feedback_age_us,feedback_sequence,can_tx_accepted,ISR_us,motor_error,feedback_raw'
$StatusNames = @('COMPLETE','USER_ABORT','RING_FULL','SD_ERROR','TIMER_ERROR','TX_FAILED','FEEDBACK_LOST','MOTOR_ERROR','POSITION_INVALID','TORQUE_LIMIT','CONTROL_DEADLINE','CAN_ERROR','START_TIMEOUT','STOP_FAILED','REFERENCE_INVALID','NONFINITE_CONTROL','RECORD_LIMIT')

 $FilesToConvert = @()
foreach ($PathItem in $BinaryFiles) {
  $ResolvedItem = Get-Item -LiteralPath $PathItem
  if ($ResolvedItem.PSIsContainer) {
    $FilesToConvert += @(Get-ChildItem -LiteralPath $ResolvedItem.FullName -Filter 'MC*.BIN' -File |
      Sort-Object LastWriteTime -Descending | Select-Object -First 2)
  } else {
    $FilesToConvert += $ResolvedItem
  }
}
if ($FilesToConvert.Count -eq 0) { throw 'No MC*.BIN files found in the supplied path(s)' }

foreach ($FileItem in $FilesToConvert) {
  $BinaryFile = $FileItem.FullName
  $InputPath = (Resolve-Path -LiteralPath $BinaryFile).Path
  $CsvPath = [IO.Path]::ChangeExtension($InputPath, '.csv')
  $ConfigPath = [IO.Path]::ChangeExtension($InputPath, '.config.json')
  $ValidationPath = [IO.Path]::ChangeExtension($InputPath, '.validation.json')
  foreach ($OutputPath in @($CsvPath, $ConfigPath, $ValidationPath)) {
    if (Test-Path -LiteralPath $OutputPath) { throw "Output already exists: $OutputPath" }
  }

  $Stream = [IO.File]::Open($InputPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
  $Reader = [IO.BinaryReader]::new($Stream)
  $Writer = $null
  try {
    if ($Stream.Length -lt 164) { throw 'File is too short for MC1 header/footer' }
    $Magic = $EncodingAscii.GetString($Reader.ReadBytes(8))
    if ($Magic -ne 'CHIRPMC1') { throw "Not a CHIRPMC1 file: $InputPath" }
    $U = [uint32[]]::new(13)
    for ($i = 0; $i -lt 13; $i++) { $U[$i] = $Reader.ReadUInt32() }
    $F = [single[]]::new(13)
    for ($i = 0; $i -lt 13; $i++) { $F[$i] = $Reader.ReadSingle() }
    $A = [uint32[]]::new(3)
    for ($i = 0; $i -lt 3; $i++) { $A[$i] = $Reader.ReadUInt32() }
    $R = [single[]]::new(2)
    for ($i = 0; $i -lt 2; $i++) { $R[$i] = $Reader.ReadSingle() }
    $P = [uint32[]]::new(3)
    for ($i = 0; $i -lt 3; $i++) { $P[$i] = $Reader.ReadUInt32() }

    $PeriodUs = $U[0]; $SweepUs = $U[1]; $PreUs = $U[2]; $PostUs = $U[3]
    $ExpectedRows = $U[4]; $RowBytes = $U[5]
    $CycleCount = $P[0] -shr 16
    if ($PeriodUs -eq 0 -or $RowBytes -ne 64 -or ($P[0] -band 0xffff) -ne 2 -or $CycleCount -eq 0) {
      throw 'Invalid MC1 period, row size, averaging policy, or cycle count'
    }
    if (($PreUs % $PeriodUs) -ne 0 -or ($SweepUs % $PeriodUs) -ne 0 -or ($PostUs % $PeriodUs) -ne 0 -or $A[0] -ne $PreUs) {
      throw 'MC1 durations are inconsistent with the sample period'
    }
    $LoggedSteps = [uint64]$CycleCount * [uint64](($PreUs + $SweepUs) / $PeriodUs)
    if ($ExpectedRows -ne ($LoggedSteps + ($CycleCount * 4) + 1)) { throw 'MC1 expectedRows is inconsistent' }

    $Stream.Position = $Stream.Length - 20
    $FooterMagic = $EncodingAscii.GetString($Reader.ReadBytes(8))
    $Rows = $Reader.ReadUInt32(); $Status = $Reader.ReadUInt32(); $StopFlags = $Reader.ReadUInt32()
    if ($FooterMagic -ne 'ENDMC001' -or $Status -ge $StatusNames.Count) { throw 'MC1 footer is invalid' }
    if ($Stream.Length -ne (144 + ([uint64]$Rows * 64) + 20) -or $Rows -gt $ExpectedRows) { throw 'MC1 file length does not match its row count' }
    if ($Status -eq 0 -and $Rows -ne $LoggedSteps) { throw 'Completed MC1 run has an unexpected row count' }

    $Config = [ordered]@{
      Hz = 1000000.0 / $PeriodUs; Period_us = $PeriodUs; Sweep_us = $SweepUs
      Pre_us = $PreUs; Post_us = $PostUs; Duration_s = $SweepUs / 1e6
      PreHold_s = $PreUs / 1e6; PostHold_s = $PostUs / 1e6
      F0_Hz = $F[0]; F1_Hz = $F[1]; A0_Nm = $F[2]; A1_Nm = $F[3]
      AmplitudeExponent = $F[4]; KP_Nm_per_deg = $F[5]; KD_Nm_per_rad_s = $F[6]
      Hold_deg = $F[7]; TorqueLimit_Nm = $F[8]; ExpectedRows = $ExpectedRows
      CycleCount = $CycleCount; PostHoldRecorded = 0; Average_us = $A[0]
      ReferenceSamples = $A[1]; AverageRows = $A[2]
      ReferenceMin_rad = $R[0]; ReferenceMax_rad = $R[1]; HoldPolicy = $P[0]
      MotorID = $U[6]; CAN_bitrate = $U[7]; NoMotor = $U[8]
      PositionPolicy = $U[9]; FeedbackTimeout_us = $U[10]
      ProfileDocumented = $U[11]; MotorTimeout_ms = $U[12]
      PositionMin_rad = $F[9]; PositionMax_rad = $F[10]
      VelocityMax_rad_s = $F[11]; MITTorqueMax_Nm = $F[12]
      CANPort = $P[1]; MotorProtocol = $P[2]
    }

    $Stream.Position = 144
    $Writer = [IO.StreamWriter]::new($CsvPath, $false, [Text.UTF8Encoding]::new($false))
    $Writer.WriteLine($Columns)
    $PreviousIndex = $null; $PreviousActual = $null
    for ($row = 0; $row -lt $Rows; $row++) {
      $Index = $Reader.ReadUInt32(); $ActualUs = $Reader.ReadUInt32(); $Phase = $Reader.ReadUInt32()
      $Floats = [single[]]::new(6)
      for ($j = 0; $j -lt 6; $j++) { $Floats[$j] = $Reader.ReadSingle() }
      $Ints = [uint32[]]::new(7)
      for ($j = 0; $j -lt 7; $j++) { $Ints[$j] = $Reader.ReadUInt32() }
      if (($Phase -ne 0) -and ($Phase -ne 1) -and ($Phase -ne 3)) { throw "Unexpected phase at row $row" }
      if (($null -ne $PreviousIndex) -and ($Index -le $PreviousIndex)) { throw "Non-increasing time index at row $row" }
      if (($null -eq $PreviousActual) -and ($ActualUs -ne 0)) { throw 'First actual_us is not zero' }
      if (($null -ne $PreviousActual) -and ($ActualUs -lt $PreviousActual)) { throw "Non-monotonic timestamp at row $row" }
      $Delta = if ($null -eq $PreviousActual) { 0 } else { $ActualUs - $PreviousActual }
      $Values = @($Index, ([uint64]$Index * $PeriodUs), $ActualUs, $Delta, $Phase) + @($Floats) + @($Ints)
      $Text = foreach ($Value in $Values) {
        if ($Value -is [single] -or $Value -is [double]) { ([double]$Value).ToString('R', $Invariant) }
        else { ([uint64]$Value).ToString($Invariant) }
      }
      $Writer.WriteLine([string]::Join(',', [string[]]$Text))
      $PreviousIndex = $Index; $PreviousActual = $ActualUs
    }
    $Writer.Dispose(); $Writer = $null
    $Config | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ConfigPath -Encoding UTF8
    $Result = [ordered]@{ valid = $true; complete = ($Status -eq 0); rows = $Rows; status = $Status; status_name = $StatusNames[$Status]; stop_flags = $StopFlags; config = $Config; errors = @() }
    $Result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ValidationPath -Encoding UTF8
    Write-Host "$([IO.Path]::GetFileName($InputPath)): $($StatusNames[$Status]), $Rows rows -> $CsvPath"
  }
  finally {
    if ($null -ne $Writer) { $Writer.Dispose() }
    $Reader.Dispose()
    $Stream.Dispose()
  }
}
