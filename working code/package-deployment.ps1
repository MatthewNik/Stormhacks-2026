$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($workspace -ne 'C:\Users\matth\Documents\Stormhacks2026 Oct 3-4') { throw 'Package only in the active workspace.' }
$buildName = 'esp32'
$binary = Join-Path $PSScriptRoot "build/$buildName/ConnectFourGameTest.ino.merged.bin"
if (-not (Test-Path -LiteralPath $binary)) { throw 'Compile the firmware successfully before packaging.' }
$options = Get-Content -LiteralPath (Join-Path $PSScriptRoot "build/$buildName/build.options.json") -Raw | ConvertFrom-Json
if ($options.customBuildProperties -notmatch 'compiler.cpp.extra_flags=-DMENU_TEST_ONLY=0(?:,|$)') {
  throw 'Full hardware package requires a normal build with MENU_TEST_ONLY=0.'
}
$deployment = Join-Path $PSScriptRoot 'deployment' 
New-Item -ItemType Directory -Force -Path $deployment | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $deployment 'firmware.bin')
$previousTemp = $env:TEMP; $previousTmp = $env:TMP
try {
  $env:TEMP = Join-Path $PSScriptRoot 'build/temp'; $env:TMP = $env:TEMP
  New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
  & python "$PSScriptRoot/package-pi.py" "$PSScriptRoot/pi" "$deployment/pi-update.zip"
  if ($LASTEXITCODE -ne 0) { throw 'Pi packaging failed.' }
} finally { $env:TEMP = $previousTemp; $env:TMP = $previousTmp }
$lines = foreach ($file in @('firmware.bin','pi-update.zip')) {
  $hash = (Get-FileHash -LiteralPath (Join-Path $deployment $file) -Algorithm SHA256).Hash.ToLower()
  "$hash  $file"
}
$lines | Set-Content -LiteralPath (Join-Path $deployment 'SHA256SUMS') -Encoding ascii
Write-Output "Ready: $deployment"
