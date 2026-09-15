# Quality of Life Series

A mod manager for [Plutonium](https://plutonium.pw). Install, update, launch and remove mods for
World at War, Black Ops and Black Ops II from one window.

<!-- screenshot: docs/home.png -->

## Download

| | |
|---|---|
| **[QualityOfLifeSeriesSetup.exe](../../releases/latest)** | Installs it like any other program — Start menu, desktop shortcut, entry in Apps & features. |
| **[QualityOfLifeSeries-portable.zip](../../releases/latest)** | Unzip and run. Installs nothing, changes nothing. |

Both are the same executable. It reads its own file name: called `…Setup.exe` it shows the
installer, otherwise it opens the app. Nothing to download twice, and the two can't disagree
about what the app does.

Windows 10 or 11, 64-bit. No .NET install needed — the runtime is inside the exe.

## What it does

- **Browse mods** — community mods with preview art, filtered by game, installed in one click.
  The list ships inside the app and refreshes from [catalog.json](catalog.json), so mods can be
  added without a new release.
- **Installed mods** — every mod in your Plutonium mods folders, per game, with size and location.
  Open a mod's folder, remove it, or add one from a `.zip`. Search when the list gets long.
- **Requirements** — what Plutonium, the games and ReShade need from Windows, and what is missing.
  It tells you; it never installs anything on your behalf.
- **Quality of Life for Black Ops II** — install or update the mod, HD textures, custom sounds and
  controller icons, each with a backup taken first.
- **Launch** — start any game through Plutonium, online or LAN, with your own player name.
- **ReShade** — install the preset collection, or run the watchdog that puts it back when
  Plutonium clears it.
- **Backups** — your original files kept in a plain folder, separate from the mod, restorable.
- **Themes** — twelve, light and dark.

Everything it changes is recorded, and anything it installs it can remove.

## Building

```powershell
dotnet publish src/QolSeriesInstaller.csproj -c Release -r win-x64 --self-contained true
```

`src/package.ps1` produces both downloads in `dist/`.

The contents of `payload/` — icon, launch scripts, ReShade presets — are zipped into the exe at
build time and unpacked beside it on first run, so a single downloaded file is a complete program.

## Credits

The mods themselves live in [T6-QoL](https://github.com/DavidHiFi/T6-QoL). ReShade is by
[crosire](https://github.com/crosire/reshade), redistributed under its own licence.

MIT licensed — see [LICENSE](LICENSE).
