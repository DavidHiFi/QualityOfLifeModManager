# Builds Quality of Life Mod Manager (Qt 6 / MinGW) into build\ and stages a runnable
# tree in dist\ with the Qt runtime beside the exe.
#   powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Clean]
# Edit the paths below if Qt, MinGW or Visual Studio live elsewhere.
param([switch] $Clean)
$ErrorActionPreference = 'Stop'

$Root   = Split-Path -Parent $PSScriptRoot
$Build  = Join-Path $Root 'build'
$Dist   = Join-Path $Root 'dist'
$Qt     = 'H:\Qt\6.8.3\mingw_64'
$MinGW  = 'H:\Qt\Tools\mingw1310_64\bin'
$VsCMake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake'
$CMake  = Join-Path $VsCMake 'CMake\bin\cmake.exe'
$Ninja  = Join-Path $VsCMake 'Ninja\ninja.exe'

# windres needs gcc reachable through PATH; put MinGW and Qt first.
$env:PATH = "$MinGW;$Qt\bin;$env:PATH"

if ($Clean -and (Test-Path $Build)) { Remove-Item $Build -Recurse -Force }

# CMake wants forward slashes in compiler paths (a backslash path breaks its cache file).
$fs = { param($p) $p -replace '\\', '/' }
& $CMake -S (& $fs $Root) -B (& $fs $Build) -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$(& $fs $Ninja)" `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$(& $fs $Qt)" `
    "-DCMAKE_CXX_COMPILER=$(& $fs $MinGW)/c++.exe" `
    "-DCMAKE_C_COMPILER=$(& $fs $MinGW)/gcc.exe" `
    "-DCMAKE_RC_COMPILER=$(& $fs $MinGW)/windres.exe" `
    "-DCMAKE_RC_FLAGS=--use-temp-file"
if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }

& $CMake --build $Build --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

$exe = Get-ChildItem $Build -Filter '*.exe' | Where-Object { $_.Name -notlike '*Probe*' } | Select-Object -First 1
if (-not $exe) { throw 'no exe produced' }

# Stage: exe + Qt runtime via windeployqt, so the result runs from dist\ directly.
if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
Copy-Item $exe.FullName $Dist
& "$Qt\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --compiler-runtime (Join-Path $Dist $exe.Name) | Out-Null
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }
Write-Host "Built: $(Join-Path $Dist $exe.Name)"
