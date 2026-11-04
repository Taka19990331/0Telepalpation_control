param(
  [string]$Matlab='C:/Program Files/MATLAB/R2024a/bin/matlab.exe',
  [ValidateSet('Both','On','Off')][string]$SdLogging='Both',
  [switch]$DesignOnly
)
$ErrorActionPreference='Stop'
$scriptPath=(Join-Path $PSScriptRoot 'verification/design_asymmetric_notch.m').Replace('\','/').Replace("'","''")
$previousPref=$env:MATLAB_PREFDIR
try {
  $env:MATLAB_PREFDIR=Join-Path $PSScriptRoot 'verification/matlabprefs'
  New-Item -ItemType Directory -Force $env:MATLAB_PREFDIR | Out-Null
  & $Matlab -wait -batch "run('$scriptPath')"
  if($LASTEXITCODE -ne 0){throw 'MATLAB design failed. Existing coefficients were not approved for this configuration.'}
} finally { $env:MATLAB_PREFDIR=$previousPref }
if(-not $DesignOnly){ & (Join-Path $PSScriptRoot 'build_verify.ps1') -SdLogging $SdLogging }
