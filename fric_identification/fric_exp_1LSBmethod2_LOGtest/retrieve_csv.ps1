param(
    [string]$Port = "COM3"
)

$ErrorActionPreference = "Stop"
$serial = $null
$tmpPath = $null

try {
    Add-Type -TypeDefinition @'
using System;
public static class LocalCrc32 {
    private static readonly uint[] Table = BuildTable();
    private static uint[] BuildTable() {
        var table = new uint[256];
        for (uint i = 0; i < 256; i++) {
            uint c = i;
            for (int j = 0; j < 8; j++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        return table;
    }
    public static uint Compute(byte[] data) {
        uint crc = 0xFFFFFFFFu;
        for (int i = 0; i < data.Length; i++) crc = Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        return crc ^ 0xFFFFFFFFu;
    }
    public static int IndexOf(byte[] data, byte[] pattern) {
        if (pattern.Length == 0) return 0;
        for (int i = 0; i <= data.Length - pattern.Length; i++) {
            int j = 0;
            while (j < pattern.Length && data[i + j] == pattern[j]) j++;
            if (j == pattern.Length) return i;
        }
        return -1;
    }
}
'@

    $serial = [System.IO.Ports.SerialPort]::new($Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    # Teensy USB CDC can deliver much faster than the default 4 KB Windows
    # serial receive queue. Enlarge it before opening the port so PowerShell's
    # validation loop never has to keep pace with individual USB packets.
    $serial.ReadBufferSize = 4194304
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $serial.ReadTimeout = 15000
    $serial.WriteTimeout = 10000
    $serial.NewLine = [string][char]10
    $serial.Open()
    $serial.DiscardInBuffer()
    $serial.Write("FETCH" + [char]10)

    $header = $null
    $headerDeadline = [DateTime]::UtcNow.AddSeconds(15)
    while ([DateTime]::UtcNow -lt $headerDeadline) {
        try { $line = $serial.ReadLine().Trim() }
        catch [System.TimeoutException] { continue }
        if ($line -eq "READY") { continue }
        if ($line.StartsWith("ERROR,")) { throw "Teensy returned $line" }
        if ($line.StartsWith("DATA,")) { $header = $line; break }
    }
    if (-not $header) { throw "Timed out waiting for the DATA header" }

    $parts = $header.Split(',')
    if ($parts.Length -ne 4) { throw "Invalid transfer header: $header" }
    $announcedSize = [long]$parts[1]
    $expectedCrc = [Convert]::ToUInt32($parts[2], 16)
    $epoch = [long]$parts[3]
    if ($announcedSize -le 0) { throw "Invalid announced size: $announcedSize" }

    $payloadStream = [System.IO.MemoryStream]::new()
    $offset = 0L
    while ($offset -lt $announcedSize) {
        $accepted = $false
        for ($attempt = 1; $attempt -le 25 -and -not $accepted; $attempt++) {
            $blockHeader = $null
            $blockHeaderDeadline = [DateTime]::UtcNow.AddSeconds(60)
            while (-not $blockHeader -and [DateTime]::UtcNow -lt $blockHeaderDeadline) {
                try { $candidate = $serial.ReadLine().Trim() }
                catch [System.TimeoutException] { continue }
                if ($candidate -eq "TRANSFER_END") { throw "Teensy ended transfer early at $offset of $announcedSize bytes" }
                if ($candidate.StartsWith("BLOCK,")) { $blockHeader = $candidate }
                elseif ($candidate.StartsWith("ERROR,")) { throw "Teensy returned $candidate" }
            }
            if (-not $blockHeader) { throw "Timed out waiting for block header at byte $offset" }
            $blockParts = $blockHeader.Split(',')
            if ($blockParts.Length -ne 4) { throw "Invalid block header: $blockHeader" }
            $blockOffset = [long]$blockParts[1]
            $blockLength = [int]$blockParts[2]
            $blockCrc = [Convert]::ToUInt32($blockParts[3], 16)
                if ($blockOffset -ne $offset -or $blockLength -le 0 -or $blockLength -gt 4096 -or ($offset + $blockLength) -gt $announcedSize) {
                throw "Unexpected block header: $blockHeader"
            }

            $block = [byte[]]::new($blockLength)
            $received = 0
            $timedOut = $false
            try {
                while ($received -lt $blockLength) {
                    $n = $serial.Read($block, $received, $blockLength - $received)
                    if ($n -gt 0) { $received += $n }
                }
            } catch [System.TimeoutException] { $timedOut = $true }
            if ($timedOut -or [LocalCrc32]::Compute($block) -ne $blockCrc) {
                $serial.Write("-")
                continue
            }
            $payloadStream.Write($block, 0, $block.Length)
            $serial.Write("+")
            $offset += $blockLength
            $accepted = $true
        }
        if (-not $accepted) { throw "Failed to receive a valid block at offset $offset after 25 attempts" }
    }
    $trailerFound = $false
    $trailerDeadline = [DateTime]::UtcNow.AddSeconds(30)
    while ([DateTime]::UtcNow -lt $trailerDeadline) {
        try { $line = $serial.ReadLine().Trim() }
        catch [System.TimeoutException] { continue }
        if ($line -eq "TRANSFER_END") { $trailerFound = $true; break }
        if ($line) { throw "Unexpected transfer trailer: $line" }
    }
    if (-not $trailerFound) { throw "Timed out waiting for transfer trailer" }
    $payload = $payloadStream.ToArray()
    $actualSize = $payload.LongLength

    $actualCrc = [LocalCrc32]::Compute($payload)
    if ($actualCrc -ne $expectedCrc) {
        throw ("CRC32 mismatch after receiving {0} bytes (announced {1}); expected {2:X8}, received {3:X8}" -f $actualSize, $announcedSize, $expectedCrc, $actualCrc)
    }

    $firstLf = [Array]::IndexOf($payload, [byte]10)
    if ($firstLf -lt 0) { throw "CSV header line missing" }
    $firstLine = [System.Text.Encoding]::ASCII.GetString($payload, 0, $firstLf).TrimEnd([char]13)
    if ($firstLine -ne "# experiment=AK80-8_1LSBfric2_fixed_steps") { throw "Unexpected CSV header: $firstLine" }
    if ([LocalCrc32]::IndexOf($payload, [System.Text.Encoding]::ASCII.GetBytes("# result=")) -lt 0) {
        throw "CSV has no completion result footer; refusing to ACK an incomplete run"
    }

    $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $dataDir = Join-Path $projectRoot "data"
    [System.IO.Directory]::CreateDirectory($dataDir) | Out-Null
    if ($epoch -gt 0) {
        $stamp = [DateTimeOffset]::FromUnixTimeSeconds($epoch).ToLocalTime().ToString("yyyyMMdd_HHmmss")
    } else {
        $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    }
    $outputPath = Join-Path $dataDir ("1LSBfric2_{0}.csv" -f $stamp)
    if ([System.IO.File]::Exists($outputPath)) {
        $outputPath = Join-Path $dataDir ("1LSBfric2_{0}_{1:X8}.csv" -f $stamp, $actualCrc)
    }
    $duplicateIndex = 2
    while ([System.IO.File]::Exists($outputPath)) {
        $outputPath = Join-Path $dataDir ("1LSBfric2_{0}_{1:X8}_{2}.csv" -f $stamp, $actualCrc, $duplicateIndex)
        $duplicateIndex++
    }

    $tmpPath = $outputPath + ".partial"
    [System.IO.File]::WriteAllBytes($tmpPath, $payload)
    [System.IO.File]::Move($tmpPath, $outputPath)
    $tmpPath = $null
    $serial.Write("ACK" + [char]10)
    $ack = $serial.ReadLine().Trim()
    if ($ack -ne "ACK_OK") { throw "Saved $outputPath, but Teensy did not acknowledge removal: $ack" }
    if ($actualSize -ne $announcedSize) {
        Write-Warning "Teensy announced $announcedSize bytes; received $actualSize. CRC and completion footer both validated."
    }
    Write-Output "Saved $outputPath ($actualSize bytes, CRC32 $($actualCrc.ToString('X8'))). $ack"
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
finally {
    if ($tmpPath -and [System.IO.File]::Exists($tmpPath)) { [System.IO.File]::Delete($tmpPath) }
    if ($serial) {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}
