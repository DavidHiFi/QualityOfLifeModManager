# Packages the Quality of Life Mod Manager release files from dist\.
#   powershell -ExecutionPolicy Bypass -File scripts\build-setup.ps1 [-NsisDir <dir>] [-OutDir <dir>]
# Builds QualityOfLifeModManagerSetup.exe (NSIS, per-user, Qt+MinGW runtime
# bundled), QualityOfLifeModManager-portable.zip, and the non-runnable-named
# in-app update payload from the same staged tree.
# Version comes from src/Version.h (CLL_VERSION). Build dist\ first with
# scripts\build.ps1.
param(
    [string] $NsisDir = $env:NSISDIR,
    [string] $OutDir = '',
    [string] $ObjDump = 'H:\Qt\Tools\mingw1310_64\bin\objdump.exe'
)
$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
if (-not $OutDir) { $OutDir = Join-Path $Root 'release' }
$Dist = Join-Path $Root 'dist'
$Stage = Join-Path ([IO.Path]::GetTempPath()) 'qol-setup-stage'

$verLine = Select-String -Path (Join-Path $Root 'src\Version.h') -Pattern '#define CLL_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"'
if (-not $verLine) { throw 'CLL_VERSION not found in src/Version.h' }
$Version = $verLine.Matches[0].Groups[1].Value

$exe = Join-Path $Dist 'QualityOfLifeModManager.exe'
if (-not (Test-Path $exe)) { throw "dist exe missing: $exe. Run scripts\build.ps1 first." }

if (-not $NsisDir) {
    foreach ($cand in @("$env:ProgramFiles\NSIS", "${env:ProgramFiles(x86)}\NSIS")) {
        if (Test-Path (Join-Path $cand 'makensis.exe')) { $NsisDir = $cand; break }
    }
}
$makensis = if ($NsisDir) { Join-Path $NsisDir 'Bin\makensis.exe' } else { $null }
if ((-not $makensis) -or (-not (Test-Path $makensis))) {
    $makensis = Join-Path $NsisDir 'makensis.exe'
}
if ((-not $NsisDir) -or (-not (Test-Path $makensis))) {
    throw 'makensis.exe not found. Install NSIS 3 (https://nsis.sourceforge.io) or pass -NsisDir.'
}

# Stage a clean tree: dist minus the runtime cache, minus any zip left
# behind by an earlier run, minus local settings (they stay per-machine).
if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
robocopy $Dist $Stage /E /XD cache /XF *.zip QualityOfLife.ini uninstall.probe /NFL /NDL /NJH /NJS | Out-Null
if ($LASTEXITCODE -ge 8) { throw "stage copy failed ($LASTEXITCODE)" }

# Fail the release before packaging if any non-Windows DLL import is absent or
# if Windows cannot start the staged app with its Qt platform and TLS plugins.
& (Join-Path $PSScriptRoot 'verify-runtime.ps1') -Dist $Stage -ObjDump $ObjDump
if ($LASTEXITCODE -ne 0) { throw "runtime verification failed ($LASTEXITCODE)" }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$setup = Join-Path $OutDir 'QualityOfLifeModManagerSetup.exe'
$zip = Join-Path $OutDir 'QualityOfLifeModManager-portable.zip'
$updatePayload = Join-Path $OutDir 'QualityOfLifeModManager.update.bin'

$env:NSISDIR = $NsisDir
& $makensis "/DDIST=$Stage" "/DVERSION=$Version" "/DOUTFILE=$setup" (Join-Path $PSScriptRoot 'setup.nsi')
if ($LASTEXITCODE -ne 0) { throw "makensis failed ($LASTEXITCODE)" }

# Extract the finished installer and run its installed runtime probe. This mode
# writes no shortcuts or registry keys and deletes its throwaway target.
$packageTestDir = Join-Path ([IO.Path]::GetTempPath()) ("qol-package-test-" + [guid]::NewGuid().ToString('N'))
$oldPackageTest = $env:QOL_PACKAGE_TEST
$oldPackageTestDir = $env:QOL_PACKAGE_TEST_DIR
try {
    $env:QOL_PACKAGE_TEST = '1'
    $env:QOL_PACKAGE_TEST_DIR = $packageTestDir
    $probe = Start-Process -FilePath $setup -ArgumentList '/S' -Wait -PassThru -WindowStyle Hidden
    if ($probe.ExitCode -ne 0) { throw "finished Setup runtime test failed ($($probe.ExitCode))" }
    if (Test-Path -LiteralPath $packageTestDir) {
        throw "finished Setup left its package-test directory behind: $packageTestDir"
    }
}
finally {
    $env:QOL_PACKAGE_TEST = $oldPackageTest
    $env:QOL_PACKAGE_TEST_DIR = $oldPackageTestDir
    if (Test-Path -LiteralPath $packageTestDir) {
        Remove-Item -LiteralPath $packageTestDir -Recurse -Force
    }
}

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath $zip -CompressionLevel Optimal
Copy-Item -LiteralPath (Join-Path $Stage 'QualityOfLifeModManager.exe') -Destination $updatePayload -Force

Write-Host "Version: $Version"
Write-Host "Setup:   $setup ($([Math]::Round((Get-Item $setup).Length / 1MB, 1)) MB)"
Write-Host "Portable: $zip ($([Math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
Write-Host "Updater:  $updatePayload ($([Math]::Round((Get-Item $updatePayload).Length / 1MB, 1)) MB)"
