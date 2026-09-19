# Installs or removes Quality of Life Mod Manager for the current user.
#   powershell -ExecutionPolicy Bypass -File install.ps1            # from a dist\ or unzipped portable folder
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
# Puts the app in %LOCALAPPDATA%\Programs\Quality of Life Mod Manager, adds a Start menu
# group, a desktop shortcut and an Apps & features entry. Settings (QualityOfLife.ini)
# stay in the install folder and survive a reinstall.
param([switch] $Uninstall, [string] $Source = $PSScriptRoot)
$ErrorActionPreference = 'Stop'

$Name    = 'Quality of Life Mod Manager'
$Exe     = 'QualityOfLifeModManager.exe'
$Dest    = Join-Path $env:LOCALAPPDATA "Programs\$Name"
$Start   = Join-Path ([Environment]::GetFolderPath('StartMenu')) "Programs\$Name"
$Desktop = [Environment]::GetFolderPath('Desktop')
$RegKey  = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager"
$ws      = New-Object -ComObject WScript.Shell

function Remove-Shortcuts {
    if (Test-Path $Start) { Remove-Item $Start -Recurse -Force }
    Remove-Item (Join-Path $Desktop "$Name.lnk") -Force -ErrorAction SilentlyContinue
    Remove-Item $RegKey -Recurse -Force -ErrorAction SilentlyContinue
}

if ($Uninstall) {
    Get-Process -Name ($Exe -replace '\.exe$','') -ErrorAction SilentlyContinue | Stop-Process -Force
    Remove-Shortcuts
    if (Test-Path $Dest) {
        # Keep the user's settings so a later reinstall picks them up.
        Get-ChildItem $Dest -Exclude 'QualityOfLife.ini' | Remove-Item -Recurse -Force
    }
    Write-Host "Removed $Name (settings kept in $Dest)."
    exit 0
}

# The source is the folder holding the exe: dist\ when run from the repo, or the
# unzipped portable folder when the script sits beside the exe.
$src = $Source
if (-not (Test-Path (Join-Path $src $Exe))) { $src = Join-Path (Split-Path -Parent $PSScriptRoot) 'dist' }
if (-not (Test-Path (Join-Path $src $Exe))) { throw "$Exe not found next to this script or in ..\dist" }

Get-Process -Name ($Exe -replace '\.exe$','') -ErrorAction SilentlyContinue | Stop-Process -Force
New-Item -ItemType Directory -Force -Path $Dest | Out-Null
robocopy $src $Dest /E /XF QualityOfLife.ini /R:3 /W:1 /NFL /NDL /NJH /NJS | Out-Null
if ($LASTEXITCODE -ge 8) { throw "copy failed ($LASTEXITCODE)" }

$target = Join-Path $Dest $Exe
$ver = (Get-Item $target).VersionInfo.FileVersion
if (-not $ver) { $ver = '2.0.0' }

New-Item -ItemType Directory -Force -Path $Start | Out-Null
foreach ($lnkPath in @((Join-Path $Start "$Name.lnk"), (Join-Path $Desktop "$Name.lnk"))) {
    $lnk = $ws.CreateShortcut($lnkPath)
    $lnk.TargetPath = $target
    $lnk.WorkingDirectory = $Dest
    $lnk.IconLocation = "$target,0"
    $lnk.Description = 'Install, update, launch and remove Call of Duty mods on Plutonium'
    $lnk.Save()
}

New-Item -Path $RegKey -Force | Out-Null
Set-ItemProperty $RegKey DisplayName $Name
Set-ItemProperty $RegKey DisplayVersion $ver
Set-ItemProperty $RegKey Publisher 'DavidHiFi'
Set-ItemProperty $RegKey DisplayIcon $target
Set-ItemProperty $RegKey InstallLocation $Dest
Set-ItemProperty $RegKey URLInfoAbout 'https://github.com/DavidHiFi/QualityOfLifeModManager'
Set-ItemProperty $RegKey UninstallString ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $Dest 'install.ps1') + '" -Uninstall')
Set-ItemProperty $RegKey NoModify 1 -Type DWord
Set-ItemProperty $RegKey NoRepair 1 -Type DWord
Set-ItemProperty $RegKey EstimatedSize ([int]((Get-ChildItem $Dest -Recurse | Measure-Object Length -Sum).Sum / 1KB)) -Type DWord
Copy-Item $PSCommandPath (Join-Path $Dest 'install.ps1') -Force

Write-Host "Installed $Name $ver to $Dest"
Write-Host "Start menu: $Start"
