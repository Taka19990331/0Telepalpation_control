param(
  [string] $Port = 'COM3',
  [ValidateSet('latest','previous')][string] $Which = 'latest',
  [string] $OutputDirectory = 'C:\0Telepalpation_control\sys_identification\captures',
  [int] $TimeoutSeconds = 7200
)

$ErrorActionPreference = 'Stop'
$null = New-Item -ItemType Directory -Force -Path $OutputDirectory
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$rawPath = Join-Path $OutputDirectory "MC1_${Which}_${stamp}_raw.txt"
$csvPath = $null
$rawWriter = [IO.StreamWriter]::new($rawPath, $false, [Text.UTF8Encoding]::new($false))
$portObject = [IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$portObject.ReadBufferSize = 4194304
$portObject.ReadTimeout = 2000
$portObject.WriteTimeout = 2000
$portObject.DtrEnable = $true
$sourceFile = $null
$csvWriter = $null
$columns = $null
$config = @{}
$count = 0
$summary = $null
$status = $null
$reason = $null
$begun = $false
$ended = $false
$previousIndex = $null
$previousActual = $null
$errors = [Collections.Generic.List[string]]::new()

try {
  $portObject.Open()
  Start-Sleep -Seconds 3
  $portObject.DiscardInBuffer()
  $command = if ($Which -eq 'latest') { 'd' } else { 'p' }
  $portObject.Write([string]$command)
  Write-Host "Reading $Which MC1 log from $Port. Output directory: $OutputDirectory"
  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  while ([DateTime]::UtcNow -lt $deadline -and -not $ended) {
    try { $line = $portObject.ReadLine().Trim() }
    catch [TimeoutException] { continue }
    if (-not $line) { continue }
    $rawWriter.WriteLine($line)
    $fields = $line.Split(',')
    try {
      if ($line.StartsWith('SOURCE_FILE,')) {
        $sourceFile = $fields[1]
        if (-not $csvPath) {
          $base = [IO.Path]::GetFileNameWithoutExtension($sourceFile)
          $csvPath = Join-Path $OutputDirectory "${base}_${stamp}.csv"
          $csvWriter = [IO.StreamWriter]::new($csvPath, $false, [Text.UTF8Encoding]::new($false))
        }
        $rawWriter.Flush()
      } elseif ($line.StartsWith('CHIRP_BEGIN,')) {
        if ($begun -or $fields.Count -lt 5 -or $fields[1] -ne 'VERSION' -or $fields[2] -ne 'MC1') { throw 'Unexpected or repeated CHIRP_BEGIN' }
        $begun = $true
        $reason = $fields[4]
        $rawWriter.Flush()
      } elseif ($line.StartsWith('CONFIG,')) {
        for ($i=1; $i+1 -lt $fields.Count; $i+=2) { $config[$fields[$i]] = [double]::Parse($fields[$i+1],[Globalization.CultureInfo]::InvariantCulture) }
        $rawWriter.Flush()
      } elseif ($line.StartsWith('RESULT,')) {
        for ($i=1; $i+1 -lt $fields.Count; $i+=2) { if ($fields[$i] -eq 'Status') { $status=[int]$fields[$i+1] } }
      } elseif ($line.StartsWith('DATA,')) {
        $values = $fields[1..($fields.Count-1)]
        if ($values[0] -eq 'index') {
          if ($csvWriter) { $csvWriter.WriteLine(($values -join ',')) }
          $columns = $values
        } else {
          if (-not $begun -or -not $columns -or -not $csvWriter -or $values.Count -ne $columns.Count) { throw 'DATA row arrived before header/config or has wrong column count' }
          $index=[uint32]$values[0]; $nominal=[uint64]$values[1]; $actual=[uint32]$values[2]; $delta=[uint32]$values[3]
          if (-not $config.ContainsKey('Period_us') -or $nominal -ne ([uint64]$index * [uint64]$config['Period_us'])) { throw "Invalid nominal time at row $count" }
          if ($null -eq $previousIndex -and $actual -ne 0) { throw 'First actual_us is not zero' }
          if ($null -ne $previousIndex -and ($index -le $previousIndex -or $actual -lt $previousActual -or $delta -ne ($actual-$previousActual))) { throw "Invalid index or timestamp at row $count" }
          $csvWriter.WriteLine(($values -join ','))
          $previousIndex=$index; $previousActual=$actual; $count++
          if (($count % 5000) -eq 0) {
            $rawWriter.Flush(); $csvWriter.Flush()
            Write-Host "Received $count rows..."
          }
        }
      } elseif ($line.StartsWith('SUMMARY,')) {
        $summary=[int]$fields[2]
      } elseif ($line -eq 'CHIRP_END') {
        $ended=$true
      } elseif ($line.StartsWith('ERROR,')) {
        throw $line
      }
    } catch {
      $errors.Add($_.Exception.Message)
      $ended = $true
      break
    }
  }
  if (-not $ended) { $errors.Add("No CHIRP_END within $TimeoutSeconds seconds") }
  if (-not $begun -or -not $columns -or -not $sourceFile) { $errors.Add('Incomplete MC1 response; this firmware may not support the requested download command') }
  if ($summary -ne $count) { $errors.Add("SUMMARY count $summary differs from received rows $count") }
  if ($reason -ne 'COMPLETE' -or $status -ne 0) { $errors.Add("Run status is $reason / $status") }
} catch {
  $errors.Add($_.Exception.Message)
} finally {
  if ($csvWriter) { $csvWriter.Dispose() }
  $rawWriter.Dispose()
  if ($portObject.IsOpen) { $portObject.Close() }
}

$result = [ordered]@{ valid = ($errors.Count -eq 0); source_file = $sourceFile; rows = $count; summary_rows = $summary; status = $status; config = $config; errors = @($errors) }
$validationPath = if ($csvPath) { [IO.Path]::ChangeExtension($csvPath,'.validation.json') } else { Join-Path $OutputDirectory "MC1_${Which}_${stamp}_validation.json" }
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $validationPath -Encoding UTF8
Write-Host "Raw: $rawPath"
if ($csvPath) { Write-Host "CSV: $csvPath" }
Write-Host "Validation: $validationPath"
if ($errors.Count) { $errors | ForEach-Object { Write-Error $_ }; exit 1 }
Write-Host "PASS: $sourceFile, $count rows"
