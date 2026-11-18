param(
  [string]$Port = 'COM3'
)

$ErrorActionPreference = 'Stop'
$fetchScript = 'C:\0Telepalpation_control\SDlog\fetch_log.ps1'

if (-not (Test-Path -LiteralPath $fetchScript -PathType Leaf)) {
  throw "共通SD取得スクリプトが見つかりません: $fetchScript"
}

# GRAVPD1スキーマを検証し、CRC確認後に C:\0Telepalpation_control\data へ保存する。
& $fetchScript -Port $Port -Which LATEST
