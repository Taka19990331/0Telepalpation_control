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
$stream = $null
$destination = $null
$stagingPath = $null
[uint64]$receivedBytes = 0
try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $serial = [System.IO.Ports.SerialPort]::new($Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.ReadTimeout = 2000
    $serial.ReadBufferSize = 1048576
    $serial.WriteTimeout = 5000
    $serial.Open()
    Start-Sleep -Milliseconds 1200
    $serial.DiscardInBuffer()
    $serial.WriteLine('d')

    $header = $null
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ([DateTime]::UtcNow -lt $deadline) {
        $line = $serial.ReadLine().Trim()
        if ($line.StartsWith('FRICTION_BIN,')) { $header = $line; break }
        if ($line.StartsWith('ERROR,')) { throw "Teensy: $line" }
    }
    if ($null -eq $header) { throw '20秒以内に転送開始ヘッダーを受信できませんでした。COM番号を確認してください。' }
    $parts = $header.Split(',')
    if ($parts.Length -ne 3 -or $parts[1] -notmatch '^FR\d{4}\.BIN$') { throw "不正な転送ヘッダー: $header" }
    [uint64]$remaining = 0
    if (-not [uint64]::TryParse($parts[2], [ref]$remaining) -or $remaining -lt 44 -or $remaining -gt 200000000) {
        throw "不正なBINサイズ: $($parts[2])"
    }
    $destination = Join-Path $OutputDirectory $parts[1]
    if (Test-Path -LiteralPath $destination) { throw "保存先はすでに存在します: $destination" }
    $stagingPath = Join-Path $OutputDirectory ($parts[1] + '.' + [Guid]::NewGuid().ToString('N') + '.partial')
    $stream = [System.IO.File]::Open($stagingPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    $buffer = New-Object byte[] 16384
    $total = $remaining
    $lastProgress = [DateTime]::UtcNow
    $nextProgress = 0.05
    while ($remaining -gt 0) {
        $available = $serial.BytesToRead
        if ($available -le 0) {
            if (([DateTime]::UtcNow - $lastProgress).TotalSeconds -gt 120) {
                throw "120秒間データを受信できません。受信済み: $receivedBytes bytes"
            }
            Start-Sleep -Milliseconds 10
            continue
        }
        $want = [int][Math]::Min([uint64][Math]::Min($buffer.Length, $available), $remaining)
        $got = $serial.Read($buffer, 0, $want)
        if ($got -le 0) { continue }
        $lastProgress = [DateTime]::UtcNow
        $stream.Write($buffer, 0, $got)
        $remaining -= [uint64]$got
        $receivedBytes += [uint64]$got
        $received = $total - $remaining
        $fraction = $received / [double]$total
        if ($fraction -ge $nextProgress) {
            Write-Progress -Activity "SDログをUSB転送中: $($parts[1])" -Status "$received / $total bytes" -PercentComplete ([int](100 * $fraction))
            $nextProgress += 0.05
        }
    }
    $stream.Flush($true)
    $stream.Dispose(); $stream = $null
    $serial.ReadTimeout = 20000
    do { $ending = $serial.ReadLine().Trim() } while ($ending -eq '')
    if ($ending -ne "FRICTION_END,$($parts[1]),$total") { throw "転送終端が一致しません: $ending" }

    $bytes = [System.IO.File]::ReadAllBytes($stagingPath)
    $magic = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 7)
    $rowBytes = [BitConverter]::ToUInt32($bytes, 8)
    $configBytes = [BitConverter]::ToUInt32($bytes, 16)
    $footerOffset = $bytes.Length - 24
    $footerMagic = [System.Text.Encoding]::ASCII.GetString($bytes, $footerOffset, 8)
    $rows = [BitConverter]::ToUInt32($bytes, $footerOffset + 8)
    $expectedLength = 20L + $configBytes + ([int64]$rows * $rowBytes) + 24L
    if ($magic -ne 'FRIC001' -or $rowBytes -ne 80 -or $configBytes -gt 4096 -or
        $footerMagic -ne 'ENDFR001' -or $bytes.Length -ne $total -or $expectedLength -ne $total) {
        throw 'BINヘッダー、フッター、または受信サイズの検査に失敗しました。'
    }
    Move-Item -LiteralPath $stagingPath -Destination $destination
    $stagingPath = $null
    Write-Host "保存しました: $destination ($total bytes)"
} catch {
    if ($stream) { $stream.Dispose(); $stream = $null }
    if ($stagingPath -and (Test-Path -LiteralPath $stagingPath)) {
        Write-Warning "途中データを保持しました: $stagingPath ($receivedBytes bytes / transfer incomplete)"
    }
    throw
} finally {
    if ($serial) { if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose() }
}
