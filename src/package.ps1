# Builds the two release downloads from one binary.
#
#   QualityOfLifeModManagerSetup.exe          double-click installer (Start menu, desktop, uninstaller)
#   QualityOfLifeModManager-portable.zip      unzip and run, installs nothing
#
# They are the same executable. Program.Main looks at its own file name: anything ending in
# "Setup" opens the install window, everything else opens the app. That is why the two downloads
# are the same size and can never disagree about what the app does.
#
# RENAMED AT v1.0.8, from QualityOfLifeSeries* - see the compatibility copy below before removing
# anything that still carries the old name.
param([string]$OutDir = "$PSScriptRoot\..\dist")

$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'QolSeriesInstaller.csproj'
$publish = Join-Path $PSScriptRoot 'bin\Release\net8.0-windows\win-x64\publish'
$modFiles = Join-Path $PSScriptRoot '..\payload'
$readme = Join-Path $PSScriptRoot '..\README.md'

Write-Host 'Publishing...'
dotnet publish $project -c Release -r win-x64 --self-contained true -v q --nologo
if ($LASTEXITCODE -ne 0) { throw 'publish failed' }

$exe = Join-Path $publish 'QualityOfLifeModManager.exe'
if (-not (Test-Path $exe)) { throw "no exe at $exe" }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
# Both names, so a dist\ left over from before the rename does not ship alongside the new files.
Get-ChildItem $OutDir -Filter 'QualityOfLife*' | Remove-Item -Recurse -Force

# 1. The installer.
Copy-Item $exe (Join-Path $OutDir 'QualityOfLifeModManagerSetup.exe') -Force

# 2. The portable zip. Mod Files ships loose here as well as inside the exe, so presets and the
#    .bat launchers are editable without unpacking anything.
$stage = Join-Path $OutDir '_portable'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item $exe $stage -Force

# ---------------------------------------------------------------------------------------------
#  COMPATIBILITY COPY - REQUIRED FOR ONE RELEASE, DO NOT DROP IT CASUALLY.
#
#  The portable zip is the self-update package. v1.0.7 and earlier are already installed on
#  people's machines, and that build looks for a file called exactly QualityOfLifeSeries.exe
#  inside the zip it downloads (InstallerService.InstallAppUpdateAsync). It is shipped; it cannot
#  be patched. If this zip contains only the new name, every existing user's in-app update fails
#  with "That download did not contain the app" and they have to reinstall by hand.
#
#  So the zip carries the same binary twice, under both names. The old build finds the old name,
#  robocopies the folder and relaunches it; the new build prefers the new name. Both exes open the
#  app - only a name ending in "Setup" starts the installer - so a user who picks the wrong one
#  still gets the right thing.
#
#  Remove this once the release that carries it has been out long enough that nobody is updating
#  from v1.0.7 or earlier.
# ---------------------------------------------------------------------------------------------
Copy-Item $exe (Join-Path $stage 'QualityOfLifeSeries.exe') -Force

# The app looks for "Mod Files" beside the exe, so keep that name in the zip.
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'Mod Files') | Out-Null
Copy-Item (Join-Path $modFiles '*') (Join-Path $stage 'Mod Files') -Recurse -Force
if (Test-Path $readme) { Copy-Item $readme $stage -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $OutDir 'QualityOfLifeModManager-portable.zip') -Force
Remove-Item $stage -Recurse -Force

Get-ChildItem $OutDir -File | ForEach-Object {
    '{0,-40} {1,8:N1} MB' -f $_.Name, ($_.Length / 1MB)
}
