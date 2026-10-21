param(
    [Parameter(Mandatory=$true)]
    [string]$Port,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $scriptDirectory = Split-Path -Parent $PSCommandPath
    $OutputDirectory = Join-Path (Split-Path -Parent $scriptDirectory) 'data'
}
$serial = $null
$rawStream = $null
$writer = $null
$stagingPath = $null
$rawPath = $null
[uint64]$receivedRows = 0
[uint64]$expectedRows = 0
[uint64]$lastProgress = 0
$sawTransferHeader = $false
$sawCsvHeader = $false
$sawEndMarker = $false
$sourceName = $null
$pending = ''
$deadline = [DateTime]::UtcNow.AddMinutes(20)
$expectedCsvHeader = 'nominal_us,index,actual_us,phase,step_index,settling_status,reason,event,target_torque_Nm,position_deg,speed_rad_s,feedback_torque_estimate_Nm,estimated_acceleration,speed_std,feedback_rx_us,feedback_age_us,feedback_sequence,motor_error,can_tx_accepted,ISR_us,period_us'
try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $serial = [System.IO.Ports.SerialPort]::new($Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.Encoding = [System.Text.Encoding]::UTF8
    $serial.ReadTimeout = 10000
    $serial.ReadBufferSize = 1048576
    $serial.WriteTimeout = 5000
    $serial.Open()
    Start-Sleep -Milliseconds 1200
    $serial.DiscardInBuffer()
    $buffer = New-Object byte[] 65536
    $decoder = [System.Text.Encoding]::UTF8.GetDecoder()
    $charBuffer = New-Object char[] 65536
    $serial.WriteLine('c')
    $rawName = 'friction_' + [DateTime]::Now.ToString('yyyyMMdd_HHmmss_fff') + '_raw.txt'
    $rawPath = Join-Path $OutputDirectory $rawName
    $rawStream = [System.IO.File]::Open($rawPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
    Write-Host 'Receiving friction CSV in blocks. Raw serial data is being saved.'

    while (-not $sawEndMarker) {
        if ([DateTime]::UtcNow -ge $deadline) { throw "Transfer timed out after receiving $receivedRows rows." }
        $available = $serial.BytesToRead
        if ($available -eq 0) { Start-Sleep -Milliseconds 2; continue }
        $count = [Math]::Min($available, $buffer.Length)
        try {
            $read = $serial.Read($buffer, 0, $count)
        } catch [System.TimeoutException] {
            continue
        }
        if ($read -le 0) { continue }
        $rawStream.Write($buffer, 0, $read)
        $charCount = $decoder.GetChars($buffer, 0, $read, $charBuffer, 0, $false)
        if ($charCount -gt 0) { $pending += [String]::new($charBuffer, 0, $charCount) }

        while (($newline = $pending.IndexOf("`n")) -ge 0) {
            $line = $pending.Substring(0, $newline).TrimEnd("`r")
            $pending = $pending.Substring($newline + 1)
            if (-not $sawTransferHeader) {
                if ($line.StartsWith('ERROR,')) { throw "Teensy: $line" }
                if (-not $line.StartsWith('FRICTION_CSV,')) { continue }
                $parts = $line.Split(',')
                if ($parts.Length -ne 3 -or $parts[1] -notmatch '^FR\d{4}\.BIN$') { throw "Invalid transfer header: $line" }
                if (-not [uint64]::TryParse($parts[2], [ref]$expectedRows) -or $expectedRows -eq 0) { throw "Invalid row count: $($parts[2])" }
                $sourceName = $parts[1]
                $csvName = [System.IO.Path]::ChangeExtension($sourceName, '.csv')
                $destination = Join-Path $OutputDirectory $csvName
                if (Test-Path -LiteralPath $destination) { throw "Destination already exists: $destination" }
                $stagingPath = Join-Path $OutputDirectory ($csvName + '.' + [Guid]::NewGuid().ToString('N') + '.partial')
                $writer = [System.IO.StreamWriter]::new($stagingPath, $false, [System.Text.UTF8Encoding]::new($false), 65536)
                $sawTransferHeader = $true
                Write-Host "Receiving $sourceName ($expectedRows rows)."
                continue
            }

            if ($line.StartsWith('FRICTION_ERROR,') -or $line.StartsWith('ERROR,')) { throw "Teensy: $line" }
            if ($line.StartsWith('FRICTION_CSV_END,')) {
                if ($line -ne "FRICTION_CSV_END,$sourceName,$expectedRows") { throw "Transfer end marker does not match: $line" }
                $sawEndMarker = $true
                break
            }
            if ($line -eq $expectedCsvHeader) {
                $sawCsvHeader = $true
                $writer.WriteLine($line)
                continue
            }
            if (-not $sawCsvHeader) {
                $writer.WriteLine($line)
                continue
            }
            if (-not $line.StartsWith('#')) {
                $fields = $line.Split(',')
                if ($fields.Length -ne 21) { throw "Malformed CSV row $($receivedRows + 1): expected 21 columns, got $($fields.Length)" }
                [uint64]$rowIndex = 0
                if (-not [uint64]::TryParse($fields[1], [ref]$rowIndex) -or $rowIndex -ne $receivedRows) {
                    throw "Nonsequential sample index on row $($receivedRows + 1): $($fields[1])"
                }
                $receivedRows++
                if ($receivedRows -ge $lastProgress + 8192) {
                    Write-Host "Received $receivedRows / $expectedRows rows"
                    Write-Progress -Activity "Transferring CSV from SD: $sourceName" -Status "$receivedRows / $expectedRows rows" -PercentComplete ([int](100 * $receivedRows / [double]$expectedRows))
                    $lastProgress = $receivedRows
                }
            }
            $writer.WriteLine($line)
        }
    }

    if (-not $sawTransferHeader -or -not $sawCsvHeader -or $receivedRows -ne $expectedRows) {
        throw "CSV row count check failed: $receivedRows / $expectedRows"
    }
    $writer.Flush()
    $writer.Dispose(); $writer = $null
    $rawStream.Flush()
    $rawStream.Dispose(); $rawStream = $null
    Move-Item -LiteralPath $stagingPath -Destination $destination
    $stagingPath = $null
    Write-Progress -Activity 'CSV transfer complete' -Completed
    Write-Host "Saved: $destination ($receivedRows rows)"
    Write-Host "Raw serial capture: $rawPath"
} catch {
    if ($writer) { $writer.Dispose(); $writer = $null }
    if ($rawStream) { $rawStream.Flush(); $rawStream.Dispose(); $rawStream = $null }
    if ($stagingPath -and (Test-Path -LiteralPath $stagingPath)) {
        Write-Warning "Partial CSV retained: $stagingPath ($receivedRows rows)"
    }
    if ($rawPath -and (Test-Path -LiteralPath $rawPath)) {
        Write-Warning "Raw serial capture retained: $rawPath"
    }
    throw
} finally {
    if ($rawStream) { $rawStream.Dispose() }
    if ($serial) { if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose() }
}
