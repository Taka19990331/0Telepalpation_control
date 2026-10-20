param(
    [Parameter(Mandatory=$true)]
    [string]$Port,
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'data')
)
$ErrorActionPreference = 'Stop'
$serial = $null
$stream = $null
$destination = $null
$fileCreated = $false
try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $serial = [System.IO.Ports.SerialPort]::new($Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.ReadTimeout = 15000
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
    $stream = [System.IO.File]::Open($destination, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    $fileCreated = $true
    $buffer = New-Object byte[] 16384
    $total = $remaining
    while ($remaining -gt 0) {
        $want = [int][Math]::Min([uint64]$buffer.Length, $remaining)
        $got = $serial.Read($buffer, 0, $want)
        if ($got -le 0) { throw 'シリアル受信が途中で終了しました。' }
        $stream.Write($buffer, 0, $got)
        $remaining -= [uint64]$got
        $received = $total - $remaining
        Write-Progress -Activity "SDログをUSB転送中: $($parts[1])" -Status "$received / $total bytes" -PercentComplete ([int](100 * $received / $total))
    }
    $stream.Flush($true)
    $stream.Dispose(); $stream = $null
    $serial.ReadTimeout = 20000
    do { $ending = $serial.ReadLine().Trim() } while ($ending -eq '')
    if ($ending -ne "FRICTION_END,$($parts[1]),$total") { throw "転送終端が一致しません: $ending" }

    $bytes = [System.IO.File]::ReadAllBytes($destination)
    $magic = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 7)
    $rowBytes = [BitConverter]::ToUInt32($bytes, 8)
    $configBytes = [BitConverter]::ToUInt32($bytes, 16)
    if ($magic -ne 'FRIC001' -or $rowBytes -ne 80 -or $bytes.Length -ne $total) { throw 'BINヘッダーまたは受信サイズの検査に失敗しました。' }
    Write-Host "保存しました: $destination ($total bytes)"
} catch {
    if ($stream) { $stream.Dispose(); $stream = $null }
    if ($fileCreated -and $destination -and (Test-Path -LiteralPath $destination)) { Remove-Item -LiteralPath $destination }
    throw
} finally {
    if ($serial) { if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose() }
}
