<div align="center">

# Quality of Life Mod Manager

**Install, update, launch and remove mods for Call of Duty on Plutonium.**

One window for the whole [Quality of Life series](https://github.com/DavidHiFi/Plutonium-QoL-Series), every other mod in your Plutonium folders, LAN and online play, ReShade, and a LAN server.

<a href="https://github.com/DavidHiFi/QualityOfLifeModManager/releases/latest">
<img src="https://img.shields.io/badge/%E2%AC%87%EF%B8%8F%20DOWNLOAD-2EA043?style=for-the-badge&labelColor=161B22" alt="Download" height="42">
</a>

<br><br>

<img src="https://img.shields.io/github/v/release/DavidHiFi/QualityOfLifeModManager?style=flat-square&color=5865F2&labelColor=161B22">
<img src="https://img.shields.io/github/downloads/DavidHiFi/QualityOfLifeModManager/total?style=flat-square&label=downloads&color=5865F2&labelColor=161B22">
<img src="https://img.shields.io/badge/platform-Windows-5865F2?style=flat-square&labelColor=161B22">

</div>

---

## What it does

* **Quality of Life page.** Install, update or remove each game's mod, plus the Black Ops II extras: HD texture pack, custom sounds, controller icons (PlayStation 5, Switch, Xbox). Everything goes into your Plutonium folder; the game's own files are never touched.
* **Play.** One page per game with the game art. LAN starts the game through Plutonium's bootstrapper with the selected mod loaded. Online hands off to Plutonium's own launcher, which signs you in. Tick **ReShade** to start the watchdog beside the game.
* **Home.** A catalog of community mods with cover art, install badges and one-click install.
* **Mods.** Drop a `.zip`, `.rar`, `.7z`, `.exe` or `.cll`, or paste a GitHub link. Every install writes a checkpoint, so uninstall restores what it replaced.
* **LAN server.** Start and stop a dedicated server with generated configs.
* **ReShade.** Cinematic colour grading for every Plutonium game. Plutonium clears its `bin` folder on every start, so the watchdog puts the files back the moment a game opens and saves your in-game preset edits.
* **Fifteen themes.** Nocturne, Classic Things, OLED, the four Catppuccin flavours and the six T3 palettes.
* **Self-updating.** Checks this repository's releases on start.

## Games

| Code | Game | Client |
| --- | --- | --- |
| T6 | Black Ops II | Plutonium |
| T4 | World at War | Plutonium |
| T5 | Black Ops | Plutonium |
| T7 | Black Ops III | T7-CLL or BOIII Community |
| IW5 | Modern Warfare 3 | Plutonium |
| S1 | Advanced Warfare | S1-CLL |

The Quality of Life series covers T6 (released), T4, T5 and T7 (planned).

## Install

1. Download **`QualityOfLifeModManagerSetup.exe`** from [Releases](https://github.com/DavidHiFi/QualityOfLifeModManager/releases/latest) and run it. It installs to `%LOCALAPPDATA%\Programs\Quality of Life Mod Manager` for the current user (no admin needed), adds Start menu and desktop shortcuts, and registers in Apps & features. Everything the app needs (Qt 6, MinGW runtime) is inside the Setup: no .NET, no VC++ redistributable, no separate Qt download.

   > Prefer no installer? Take `QualityOfLifeModManager-portable.zip` and unzip it anywhere writable.
   >
   > Either way, do **not** download the bare `QualityOfLifeModManager.exe` for a fresh install. It is the in-app updater's file only - the exe without its DLLs - and running it alone fails with `Qt6Gui.dll was not found`. No redistributable fixes that; Windows reports the missing DLL before the app's own code runs, so it cannot warn you itself.

2. Run the app. The first-run wizard asks for your name, theme, Plutonium folder and game folders.
3. Open **Quality of Life** in the sidebar and press **Install** on Black Ops II.
4. Open **Black Ops II**, pick LAN or Online, and press Start.

Prefer a zip? `QualityOfLifeModManager-portable.zip` is the same app: unzip anywhere writable and run the exe. The bare `QualityOfLifeModManager.exe` asset is the in-app updater's file only - running it alone fails with `Qt6Gui.dll was not found` because its runtime ships beside it, not inside it.

Settings live in `QualityOfLife.ini` next to the exe. Point it at a different Plutonium folder from **Settings** if yours is not `%LOCALAPPDATA%\Plutonium`.

## Online versus LAN

Plutonium's bootstrapper, started directly, has no login and answers `Could not authenticate (401)`. LAN mode needs no login, so the app can pre-load a mod with `fs_game`. Online play therefore goes through the `plutonium://play/<game>` handler that Plutonium's launcher registers: the launcher signs in and starts the game, and you pick the mod from its in-game Mods menu. Online is only offered for the registered install in `%LOCALAPPDATA%\Plutonium`.

## Building

Qt 6 (Widgets, Network, Concurrent, Svg), a MinGW kit and CMake.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Clean
```

The script writes `dist\` with the exe and the Qt runtime. Paths at the top of the script point at the Qt kit, MinGW and the Visual Studio folder that carries cmake and ninja. See `scripts\README.txt` for the static single-exe route.

Version lives in `src/Version.h`.

## Feeds

* `qol_home.json` is the Home catalog. Edit it, commit, and the app picks it up on next start; a copy is built into the exe as a fallback.
* `qol_update.json` is the update feed. Regenerate it with `python scripts\gen_update.py -o qol_update.json` after each release.

## Credits

* [DavidHiFi](https://github.com/DavidHiFi), this app and the Quality of Life series.
* [MestreTM](https://github.com/MestreTM/CLL-Cod-Lan-Launcher), Cod Lan Launcher, which this app is forked from. Its games list, mod installer, LAN server, themes and updater are all its work.
* [JugAndDoubleTap](https://github.com/JugAndDoubleTap/LanLauncher), the original LanLauncher.
* [xerxes-at](https://github.com/xerxes-at), T4, T5 and T6 dedicated server configs.
* [alterware.dev](https://alterware.dev) and [boiii-community](https://gitlab.com/boiii-community/BOIII-Community), standalone client sources.
* [crosire](https://reshade.me), ReShade.
* Plutonium and Call of Duty belong to their owners. This project is not affiliated with them.

## License

LGPL-3.0, inherited from Cod Lan Launcher. See [LICENSE](LICENSE).
