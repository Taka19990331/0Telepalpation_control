param(
    [Parameter(Mandatory=$true)][string]$Port,
    [switch]$Start,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'data')
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$serial = [System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.ReadTimeout = 1000
$serial.WriteTimeout = 5000
$serial.NewLine = "`n"
try {
    $serial.Open()
    Start-Sleep -Seconds 2
    $serial.DiscardInBuffer()
    if ($Start) {
        $serial.Write('s')
        Write-Host 'VelRamp started. Recording at 2 kHz; expected profile duration is about 112 seconds.'
        Start-Sleep -Seconds 118
    }
    $serial.Write('d')
    $lines = [System.Collections.Generic.List[string]]::new()
    $receiving = $false
    $expectedRows = -1
    $sourceName = ''
    $deadline = [DateTime]::UtcNow.AddMinutes(5)
    while ([DateTime]::UtcNow -lt $deadline) {
        try { $line = $serial.ReadLine().TrimEnd("`r") }
        catch [System.TimeoutException] { continue }
        if (-not $receiving) {
            if ($line.StartsWith('VELRAMP_BEGIN,')) {
                $parts = $line.Split(',')
                if ($parts.Length -ne 3 -or $parts[1] -notmatch '^VR\d{4}\.BIN$') { throw "Invalid transfer header: $line" }
                $sourceName = $parts[1]
                $expectedRows = [int]$parts[2]
                $receiving = $true
                continue
            }
            continue
        }
        if ($line.StartsWith('VELRAMP_END,')) {
            $parts = $line.Split(',')
            if ($parts.Length -ne 3 -or $parts[1] -ne $sourceName -or [int]$parts[2] -ne $expectedRows) {
                throw "Transfer end marker mismatch: $line"
            }
            break
        }
        $lines.Add($line)
    }
    if (-not $receiving -or $lines.Count -lt 2) { throw 'Timed out before receiving CSV content.' }
    $headerIndex = -1
    for ($i=0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -eq 'timestamp_us,command_rad_s,position_rad,velocity_rad_s,torque_estimate_nm,i_torque_command_nm,stage,error_code,feedback_valid') {
            $headerIndex = $i; break
        }
    }
    if ($headerIndex -lt 0) { throw 'CSV column header was not received.' }
    $rowCount = $lines.Count - $headerIndex - 1
    if ($rowCount -ne $expectedRows) { throw "Row count mismatch: received $rowCount, expected $expectedRows." }
    $stem = 'VelRamp_' + [DateTime]::Now.ToString('yyyyMMdd_HHmmss')
    $destination = Join-Path $OutputDirectory ($stem + '.csv')
    $suffix = 1
    while (Test-Path -LiteralPath $destination) {
        $destination = Join-Path $OutputDirectory ($stem + '_' + $suffix + '.csv')
        $suffix++
    }
    [System.IO.File]::WriteAllLines($destination,$lines.ToArray(),[System.Text.UTF8Encoding]::new($false))
    Write-Host "Saved $rowCount rows from $sourceName to $destination"
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}

