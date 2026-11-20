param(
  [ValidateSet('Both','On','Off')][string]$SdLogging='Both',
  [string]$ArduinoCli='C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'
)
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$buildRoot=Join-Path $workspace 'build'
$tempRoot=Join-Path $buildRoot 'trans_ctrl_notch5_tmp'
New-Item -ItemType Directory -Force $tempRoot | Out-Null
$previousTemp=$env:TEMP; $previousTmp=$env:TMP
try {
  $env:TEMP=$tempRoot; $env:TMP=$tempRoot
  $modes=if($SdLogging -eq 'Both'){@('On','Off')}else{@($SdLogging)}
  foreach($mode in $modes) {
    $build=Join-Path $buildRoot ('trans_ctrl_notch5_sd_'+$mode.ToLowerInvariant())
    $arguments=@('compile','--clean','--fqbn','teensy:avr:teensy41','--build-path',$build)
    $config=Join-Path $buildRoot 'diagnostics-arduino-cli.yaml'
    if(Test-Path $config){$arguments+=@('--config-file',$config)}
    if($mode -eq 'Off') {
      # Teensy platform.txt uses build.flags.defs, not build.extra_flags.
      $arguments+=@('--build-property','build.flags.defs=-D__IMXRT1062__ -DTEENSYDUINO=160 -DTRANS_ENABLE_SD_LOGGING=0')
    }
    $arguments+=$PSScriptRoot
    & $ArduinoCli @arguments 2>&1 | Tee-Object (Join-Path $buildRoot ('trans_ctrl_notch5_build_sd_'+$mode.ToLowerInvariant()+'.txt'))
    if($LASTEXITCODE -ne 0){throw "Teensy build failed: SD $mode"}
  }
} finally {$env:TEMP=$previousTemp; $env:TMP=$previousTmp}




