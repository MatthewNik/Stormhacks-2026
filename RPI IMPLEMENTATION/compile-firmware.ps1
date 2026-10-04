param([string]$Cli = 'C:/Users/matth/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($projectRoot -ne 'C:\Users\matth\Documents\Stormhacks2026 Oct 3-4') { throw 'Build only in the active hackathon workspace.' }
$buildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'build'))
if (-not $buildRoot.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar)) { throw 'Invalid build location.' }
$previousTemp = $env:TEMP; $previousTmp = $env:TMP
try {
  $env:TEMP = Join-Path $buildRoot 'temp'; $env:TMP = $env:TEMP
  New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
  & $Cli compile --jobs 1 --fqbn esp32:esp32:esp32 --config-file "$PSScriptRoot/arduino-cli.yaml" --libraries 'C:/Users/matth/Documents/Arduino/libraries' --build-path "$buildRoot/esp32" --build-property compiler.cache_core=false "$PSScriptRoot/firmware/ConnectFourGameTest"
  if ($LASTEXITCODE -ne 0) { throw 'ESP32 compile failed.' }
} finally { $env:TEMP = $previousTemp; $env:TMP = $previousTmp }
