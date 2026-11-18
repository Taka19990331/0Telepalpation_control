param(
  [Parameter(Mandatory = $true)]
  [ValidatePattern('^COM[0-9]+$')]
  [string]$Port,

  [string]$OutputDirectory = (Join-Path $PSScriptRoot 'logs')
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$logPath = Join-Path $OutputDirectory "dual_zero_$stamp.log"
$serial = [System.IO.Ports.SerialPort]::new(
  $Port, 115200, [System.IO.Ports.Parity]::None, 8,
  [System.IO.Ports.StopBits]::One
)
$serial.ReadTimeout = 500
$serial.ReadBufferSize = 16777216
$serial.NewLine = "`n"
$faultSeen = $false
$dumpComplete = $false
$writer = $null

try {
  $serial.Open()
  $writer = [System.IO.StreamWriter]::new(
    $logPath, $false, [System.Text.UTF8Encoding]::new($false)
  )
  $writer.AutoFlush = $true
  Write-Host "Capturing $Port to $logPath. Stop manually with Ctrl+C."

  while ($true) {
    try {
      $line = $serial.ReadLine().TrimEnd("`r")
    } catch [System.TimeoutException] {
      continue
    }

    $record = '{0},{1}' -f [DateTime]::UtcNow.ToString('o'), $line
    $writer.WriteLine($record)
    Write-Host $line

    if ($line.StartsWith('FAULT,')) { $faultSeen = $true }
    if ($line -eq 'HISTORY_END') { $dumpComplete = $true }
    if ($faultSeen -and $dumpComplete -and $line -eq 'HALTED,RESET_REQUIRED') {
      break
    }
  }
} finally {
  if ($writer) { $writer.Dispose() }
  if ($serial.IsOpen) { $serial.Close() }
  $serial.Dispose()
}

if ($faultSeen) {
  Write-Host "Fault captured. Review the log: $logPath" -ForegroundColor Yellow
  exit 2
}
