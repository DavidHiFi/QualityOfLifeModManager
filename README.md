<div align="center">

# Quality Of Life Series

**The mod manager for Call of Duty Zombies on [Plutonium](https://plutonium.pw).**

Install, update, launch and remove mods for World at War, Black Ops and Black Ops II from one window, including every mod in the [Quality Of Life series](https://github.com/DavidHiFi/Plutonium-QoL-Series).

<a href="../../releases/latest">
<img src="https://img.shields.io/badge/%E2%AC%87%EF%B8%8F%20DOWNLOAD%20LATEST%20RELEASE-2EA043?style=for-the-badge&labelColor=161B22" alt="Download the latest release" height="42">
</a>

<br><br>

<img src="https://img.shields.io/github/v/release/DavidHiFi/QualityOfLifeSeries?style=flat-square&label=version&color=5865F2&labelColor=161B22">
<img src="https://img.shields.io/github/downloads/DavidHiFi/QualityOfLifeSeries/total?style=flat-square&label=downloads&color=5865F2&labelColor=161B22">
<img src="https://img.shields.io/badge/platform-Windows-5865F2?style=flat-square&labelColor=161B22">

</div>

<!-- screenshot: docs/home.png -->

---

## Download

| | |
|---|---|
| **[QualityOfLifeSeriesSetup.exe](../../releases/latest)** | Installs it like any other program, with a Start menu entry, a desktop shortcut and an entry in Apps & features. |
| **[QualityOfLifeSeries-portable.zip](../../releases/latest)** | Unzip and run. Installs nothing, changes nothing. |

Both are the same executable. It reads its own file name: called `…Setup.exe` it shows the installer, otherwise it opens the app. Nothing to download twice, and the two can't disagree about what the app does.

Windows 10 or 11, 64-bit. No .NET install needed, the runtime is inside the exe.

---

## What it does

* **Browse mods:** community mods with preview art, filtered by game, installed in one click. The list ships inside the app and refreshes from [catalog.json](catalog.json), so mods can be added without a new release.
* **Installed mods:** every mod in your Plutonium mods folders, per game, with size and location. Open a mod's folder, remove it, or add one from a `.zip`. Search when the list gets long.
* **Requirements:** what Plutonium, the games and ReShade need from Windows, and what is missing. It tells you; it never installs anything on your behalf.
* **The Quality Of Life mods:** install or update any mod in the series, with its HD textures, custom sounds and controller icons, each with a backup taken first.
* **Launch:** start any game through Plutonium, online or LAN, with your own player name.
* **ReShade:** install the preset collection, or run the watchdog that puts it back when Plutonium clears it.
* **Backups:** your original files kept in a plain folder, separate from the mod, restorable.
* **Themes:** twelve, light and dark.

Everything it changes is recorded, and anything it installs it can remove.

---

## The series

| Game | Mod | In this app |
|---|---|---|
| Call of Duty: Black Ops II | [T6-QoL](https://github.com/DavidHiFi/T6-QoL) | Ready now |
| Call of Duty: Black Ops | [T5-QoL](https://github.com/DavidHiFi/T5-QoL) | When it is built |
| Call of Duty: World at War | [T4-QoL](https://github.com/DavidHiFi/T4-QoL) | When it is built |
| Call of Duty: Black Ops III | [T7-QoL](https://github.com/DavidHiFi/T7-QoL) | No, Black Ops III does not run on Plutonium |

[Plutonium-QoL-Series](https://github.com/DavidHiFi/Plutonium-QoL-Series) is the hub for all of them.

---

## Building

```powershell
dotnet publish src/QolSeriesInstaller.csproj -c Release -r win-x64 --self-contained true
```

`src/package.ps1` produces both downloads in `dist/`.

The contents of `payload/`, the icon, launch scripts and ReShade presets, are zipped into the exe at build time and unpacked beside it on first run, so a single downloaded file is a complete program.

---

## Credits

Written by [DavidHiFi](https://github.com/DavidHiFi). The mods themselves live in their own repositories, linked in the table above.

ReShade is by [crosire](https://github.com/crosire/reshade), redistributed under its own licence.

MIT licensed, see [LICENSE](LICENSE).
