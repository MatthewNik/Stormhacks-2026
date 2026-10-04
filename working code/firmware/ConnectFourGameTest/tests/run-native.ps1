$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$expected = 'C:\Users\matth\Documents\Stormhacks2026 Oct 3-4'
if ($workspace -ne $expected) { throw 'Run these tests inside the active hackathon workspace.' }
$versionRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$output = Join-Path $versionRoot 'build/native'
$temp = Join-Path $versionRoot 'build/temp'
New-Item -ItemType Directory -Force -Path $output,$temp | Out-Null
$oldTemp = $env:TEMP; $oldTmp = $env:TMP
try {
  $env:TEMP = $temp; $env:TMP = $temp
  Push-Location $workspace
  try {
    $timed = 'native_timed_robot_tests'
    $ir = 'native_tests'
    foreach ($name in @($ir,$timed,'pca_hardware_tests','sensor_capture_tests','sidecar_tests','menu_tests')) {
      [string[]]$flags = if ($name -eq 'menu_tests') { @() } elseif ($name -eq $ir) { @('/DMENU_TEST_ONLY=0','/DROBOT_IR_CONFIRM_BUILD=1') } else { @('/DMENU_TEST_ONLY=0') }
      $source = if ($name -eq $timed) { 'native_tests' } else { $name }
      & cl @flags /nologo /std:c++17 /EHsc /W4 /I "$PSScriptRoot/stubs" "$PSScriptRoot/$source.cpp" "/Fo$output/$name.obj" "/Fe$output/$name.exe"
      if ($LASTEXITCODE -ne 0) { throw "Compile failed: $name" }
      & "$output/$name.exe"
      if ($LASTEXITCODE -ne 0) { throw "Tests failed: $name" }
    }
  } finally { Pop-Location }
} finally { $env:TEMP = $oldTemp; $env:TMP = $oldTmp }
