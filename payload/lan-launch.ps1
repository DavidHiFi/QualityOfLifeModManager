<#
================================================================================
  Quality Of Life Series - one-click game launcher

  Started life as a Black Ops II LAN launcher (2026-08-26). It now starts
  Black Ops II, Black Ops or World at War through Plutonium, either normally
  or in LAN mode, and can load the zm_qol mod automatically in LAN mode.

  Launched by "Play BO2 with mod (LAN).bat" and "Play BO2 with mod + ReShade
  (LAN).bat" (both sit in this same "Mod Files" folder), and by the app's own
  Play rows, which pass the game, mode and name.

  WHY LAN MODE FOR THE MOD: an asset mod (mod.ff/mod.iwd) can only be
  auto-loaded with the engine's fs_game mechanism, and the bootstrapper started
  directly has no Plutonium login - an ONLINE boot needs that login and answers
  "Could not authenticate to Plutonium (401)" without it. LAN mode needs no
  login, so fs_game works and the mod is already running the moment Zombies
  loads. Online launches therefore go through the launcher's own authenticated
  route (plutonium://play/<game>); pick Quality Of Life from Zombies -> Mods
  if you want it there.

  THE GAME PATH IS READ FROM PLUTONIUM'S OWN CONFIG, NOT HARDCODED.
  %LOCALAPPDATA%\Plutonium\config.json is a file Plutonium itself writes and
  maintains - every Plutonium install has one, and its t4Path/t5Path/t6Path
  keys are wherever THAT PC's games actually live.

  Closing the game window ends the session normally. If you started the
  ReShade watchdog alongside it (-Watchdog), closing THAT window just stops
  ReShade being restored - nothing is uninstalled either way.
================================================================================
#>

param(
    [string] $PlutoRoot = (Join-Path $env:LOCALAPPDATA 'Plutonium'),
    [string] $Mod       = 'zm_qol',
    # In-game name passed with +name. The app fills this from Settings ->
    # Player name; left generic here on purpose (no personal machine
    # specifics ship).
    [string] $LanName   = '',
    [ValidateSet('t4', 't5', 't6')] [string] $Game = 't6',
    [ValidateSet('zm', 'mp')] [string] $Play = 'zm',
    # -Online starts the game normally (no -lan, no fs_game). Without it the
    # launch is a LAN session, which is what the two .bat files do.
    [switch] $Online,
    # Also start the ReShade watchdog (reshade-watchdog.ps1, same folder) in
    # its own window alongside the game.
    [switch] $Watchdog,
    # Run with no prompts: print the reason and exit non-zero on failure
    # instead of waiting for Enter. The mod manager runs it this way and shows
    # the message itself; the .bat files leave it off so the window stays open.
    [switch] $Quiet
)

$ErrorActionPreference = 'Stop'
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Definition }

function Fail {
    param([string] $Message)
    Write-Host ''
    Write-Host "  $Message" -ForegroundColor Red
    Write-Host ''
    if (-not $Quiet) { Read-Host 'Press Enter to close' }
    exit 1
}

$Names = @{ t4 = 'World at War'; t5 = 'Black Ops'; t6 = 'Black Ops II' }
$Modes = @{ zm = 'Zombies'; mp = 'Multiplayer' }
$GameName = $Names[$Game]
$ModeName = $Modes[$Play]

# Every Plutonium game slot has its own process name, and Zombies on T4/T5 is
# the single-player exe (t4sp/t5sp). The old "$Game$Play" id built t4zm and
# t5zm, which no Plutonium game ever uses: the bootstrapper printed its usage
# line and quit, which is why the window "flashed" and no game appeared.
$Ids = @{
    t6 = @{ zm = 't6zm'; mp = 't6mp' }
    t5 = @{ zm = 't5sp'; mp = 't5mp' }
    t4 = @{ zm = 't4sp'; mp = 't4mp' }
}
$GameId = $Ids[$Game][$Play]

$BinDir = Join-Path $PlutoRoot 'bin'
$Boot   = Join-Path $BinDir 'plutonium-bootstrapper-win32.exe'
$Config = Join-Path $PlutoRoot 'config.json'

if (-not (Test-Path -LiteralPath $Boot)) {
    Fail "Plutonium isn't installed where expected ($Boot missing). Run Plutonium at least once first."
}
if (-not (Test-Path -LiteralPath $Config)) {
    Fail "Plutonium's config.json wasn't found ($Config). Open Plutonium and load $GameName at least once first, so it knows where the game is."
}

$GamePath = $null
$PathKey = "${Game}Path"
try {
    $cfg = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
    $GamePath = [string] $cfg.$PathKey
} catch {
    Fail "Couldn't read config.json: $($_.Exception.Message)"
}
if (-not $GamePath -or -not (Test-Path -LiteralPath $GamePath)) {
    Fail "Plutonium's config.json doesn't list a valid $GameName folder. Open Plutonium and load $GameName at least once first."
}

# Already running? Don't fight a second instance over the same game slot.
$already = @('plutonium-bootstrapper-win32', 't4sp', 't4mp', 't5sp', 't5mp', 't6zm', 't6mp', 'iw5mp') |
    ForEach-Object { Get-Process -Name $_ -ErrorAction SilentlyContinue } |
    Select-Object -First 1
if ($already) {
    Fail "$GameName already appears to be running. Close it first, then try again."
}

$InGameName = if ($LanName -and $LanName.Trim().Length -gt 0) { $LanName.Trim() } else { 'Player' }
$gameArgs = @($GameId, ('"' + $GamePath.TrimEnd('\') + '"'), '+name', ('"' + $InGameName + '"'))
if (-not $Online) {
    $gameArgs += '-lan'
    if ($Game -eq 't6' -and $Play -eq 'zm') {
        $gameArgs += @('+set', 'fs_game', ('"mods/' + $Mod + '"'))
    }
}

#  An ONLINE launch needs a Plutonium login session. The bootstrapper started
#  directly has none: Plutonium itself reports "Could not authenticate to
#  Plutonium (401)" and closes. LAN needs no login. So online play must go
#  through the launcher's own authenticated route - the plutonium://play/<id>
#  protocol handler, the same one its forum staff give out for direct launches
#  (forum topic 41447). A fresh launcher session does the login; no token is
#  read, stored or replayed here.
if ($Online) {
    $DefaultRoot = Join-Path $env:LOCALAPPDATA 'Plutonium'
    if ([IO.Path]::GetFullPath($PlutoRoot).TrimEnd('\') -ne [IO.Path]::GetFullPath($DefaultRoot).TrimEnd('\')) {
        Fail 'Online launch is disabled for an alternate Plutonium root. It would use the registered installation instead.'
    }
    Write-Host ''
    Write-Host "  Starting $GameName $ModeName through Plutonium's launcher..." -ForegroundColor Cyan
    Write-Host '  A launcher window handles the online login, then the game opens.' -ForegroundColor Yellow
    Write-Host ''
    try {
        Start-Process -FilePath "plutonium://play/$GameId" -WorkingDirectory $PlutoRoot -ErrorAction Stop | Out-Null
    } catch {
        Fail "Couldn't hand the game to Plutonium: $($_.Exception.Message)"
    }
    if ($Watchdog) {
        $watchdogPS1 = Join-Path $ScriptDir 'reshade-watchdog.ps1'
        if (Test-Path -LiteralPath $watchdogPS1) {
            Start-Process -FilePath 'powershell.exe' `
                -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$watchdogPS1`"", '-PlutoRoot', "`"$PlutoRoot`"") `
                -WorkingDirectory $PlutoRoot -WindowStyle Normal | Out-Null
            Write-Host '  ReShade watchdog started in its own window - leave it open while you play.' -ForegroundColor Green
        } else {
            Write-Host '  reshade-watchdog.ps1 is missing from this folder - ReShade will not be restored.' -ForegroundColor Yellow
        }
    }
    Write-Host '  Game launching - this window can be closed.' -ForegroundColor Green
    Start-Sleep -Seconds 3
    exit 0
}

Write-Host ''
if (-not $Online) {
    Write-Host "  Starting $GameName $ModeName in LAN mode as '$InGameName'" -NoNewline -ForegroundColor Cyan
    if ($Game -eq 't6' -and $Play -eq 'zm') { Write-Host " with '$Mod' loaded..." -ForegroundColor Cyan } else { Write-Host '...' -ForegroundColor Cyan }
    Write-Host '  Offline only this session - no online servers or stats.' -ForegroundColor Yellow
    Write-Host ''
}

try {
    Start-Process -FilePath $Boot -ArgumentList $gameArgs -WorkingDirectory $PlutoRoot -ErrorAction Stop | Out-Null
} catch {
    Fail "Couldn't start the game: $($_.Exception.Message)"
}

if ($Watchdog) {
    $watchdogPS1 = Join-Path $ScriptDir 'reshade-watchdog.ps1'
    if (Test-Path -LiteralPath $watchdogPS1) {
        Start-Process -FilePath 'powershell.exe' `
            -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$watchdogPS1`"", '-PlutoRoot', "`"$PlutoRoot`"") `
            -WorkingDirectory $PlutoRoot -WindowStyle Normal | Out-Null
        Write-Host '  ReShade watchdog started in its own window - leave it open while you play.' -ForegroundColor Green
    } else {
        Write-Host '  reshade-watchdog.ps1 is missing from this folder - ReShade will not be restored.' -ForegroundColor Yellow
        Write-Host '  Reinstall from the full download, or run the installer''s ReShade option first.' -ForegroundColor Yellow
    }
}

Write-Host ''
Write-Host '  Game launching - this window can be closed.' -ForegroundColor Green
Write-Host ''
Start-Sleep -Seconds 3