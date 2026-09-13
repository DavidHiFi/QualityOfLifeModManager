# Cod Lan Launcher (CLL)

![Cod Lan Launcher](https://i.imgur.com/xHyfkQH.png)

Supported languages:

![English](https://img.shields.io/badge/lang-English-blue)
![Português](https://img.shields.io/badge/lang-Portugu%C3%AAs-green)
![Español](https://img.shields.io/badge/lang-Espa%C3%B1ol-yellow)
![Русский](https://img.shields.io/badge/lang-%D0%A0%D1%83%D1%81%D1%81%D0%BA%D0%B8%D0%B9-red)

## [>> Download in Releases <<](https://github.com/MestreTM/CLL-Cod-Lan-Launcher/releases)

---

Offline launcher for **Call of Duty** on LAN mode — no login, no official client left open. From **v1.2.0** it also launches **Advanced Warfare** and **Black Ops III** with standalone clients.

Made by **MestreTM**. Original idea: LanLauncher by [JugAndDoubleTap](https://github.com/JugAndDoubleTap/LanLauncher).

Repository: [github.com/MestreTM/CLL-Cod-Lan-Launcher](https://github.com/MestreTM/CLL-Cod-Lan-Launcher)

The code is open source. The portable `.exe` is on [Releases](../../releases), not in this tree.

App version lives in one place: `src/Version.h` (`CLL_VERSION`).

## Games

| Code | Game | Client |
| --- | --- | --- |
| T4 | Call of Duty: World at War | Plutonium |
| T5 | Call of Duty: Black Ops | Plutonium |
| T6 | Call of Duty: Black Ops II | Plutonium |
| IW5 | Call of Duty: Modern Warfare 3 | Plutonium |
| S1 | Call of Duty: Advanced Warfare | S1-CLL (`s1.exe`) |
| T7 | Call of Duty: Black Ops III | T7-CLL or BOIII Community |

Modes: multiplayer and zombies on Plutonium titles. Advanced Warfare also has campaign and survival. Black Ops III always starts with `-launch` (no separate binaries).

## What the program does

- First-run wizard: language + theme, Plutonium Portable or an existing install, nickname, game folders (scrollable)
- Themes (Nocturne, Classic Things dark/light) chosen in the wizard and in Settings
- Steam folder detection (registry + `libraryfolders.vdf`)
- **Home** catalog of mods (remote `cll_home.json`), with installed-state badges and image cache
- Sidebar: Home first (can be hidden in Settings), games list you can reorder, tools
- Launch / stop the game process (including S1 and T7 clients)
- **Plutonium Portable** (`pu.dat`) into `./pu`, or `%LOCALAPPDATA%\Plutonium`
- **Mods**: zip / rar / 7z / exe / `.cll`, drag-and-drop, GitHub / custom host manifests, compressed packs, progress, uninstall + rollback
- LAN dedicated server (beta): start/stop, configs, local IPs
- Built-in updater against `cll_update.json` (launcher, portable kit, T7-CLL, S1-CLL, BOIII Community)
- Languages: English, Portuguese, Spanish, Russian (no restart)
- One portable exe (`CodLanLaucher.exe`) with art, icons, themes and translations embedded

## Home catalog

`Home` loads [cll_home.json](https://mestretm.github.io/CLL-Cod-Lan-Launcher/cll_home.json). Cards show cover, game, author and an installed badge when the mod is already on disk.

**Install** / **Details** follow the language. Covers can be local files or `https://` URLs.

Toggle **Show Home tab** in Settings if you want the catalog hidden.

## Mods

### Manual import

On **Mods**, drop or pick a `.zip`, `.rar`, `.7z`, `.exe`, `.cll`, or paste a GitHub / custom host link.

Generic archives without a CLL manifest still open the installer. Mixed packs (`storage/` → Plutonium, `steam/` → game folder) are sorted automatically. Confirm the plan; replaced files can be backed up.

### CLL packs (`.cll` or zip with `cll_installer.json`)

| Field | Meaning |
| --- | --- |
| `name`, `version`, `author`, `description` | Shown in the install dialog |
| `game` | `t4`, `t5`, `t6`, `iw5` |
| `folders[].from` | Folder inside the archive |
| `folders[].to` | `pu_folder` or `game_folder` |
| `folders[].dest` | Relative path under that root |

Sample: `docs/cll_installer.example.json`.

### GitHub / custom host

Paste a repo URL or a direct `manifest.json` link. The launcher reads `/releases/latest/download/manifest.json` (or your host URL), then `mod.json`, queues files, retries failed downloads, and stores `download.json` next to the mod for later diffs (changed / missing / deleted).

Compressed releases (`bundle` → `pack.zip` / `pack.7z`) extract `Plutonium/` into the Plutonium instance and `steam/` into the game folder. Roots use `PLUTO_T6` / `PLUTO_T5` / `PLUTO_T4` / `PLUTO_IW5`.

If a release contains `.exe` / `.dll`, a warning is shown (origin is not verified). Failed files can be copied as URL + destination for a manual retry.

### Uninstall

Each install writes a checkpoint. Uninstall deletes new files, restores overwritten ones, then drops the backup.

## Black Ops III clients

Gear next to **Start** (same height as Start):

1. **T7-CLL** (default) — [MestreTM/t7-cll](https://github.com/MestreTM/t7-cll) latest `t7_cll.zip`, extracted into the game root. LAN-oriented, no watermark.
2. **Competitive — (Boiii-Community)** — `boiii.exe` from the community `updater.json` (SHA-1).

First Start downloads T7-CLL after an OK confirmation. Switching clients in the gear dialog shows “download complete”. Competitive has an extra tab for launch arguments (empty by default, with Reset). T7-CLL defaults include `-launch -noconsole -nowatermark -nointro`. A small watcher dismisses the community Error dialog (`#32770`).

## Advanced Warfare

First Start can download [S1-CLL](https://github.com/MestreTM/s1-cll) `s1.exe` into the game folder (`releases/latest`). Launch flags: `-multiplayer` / `-zombies` / `-singleplayer` / `-survival`, plus `-noconsole -nowatermark`.

## Updates

Feed: [cll_update.json](https://mestretm.github.io/CLL-Cod-Lan-Launcher/cll_update.json)

| Item | How it is compared |
| --- | --- |
| Cod Lan Launcher | Version in `src/Version.h` vs tag in the feed |
| `pu.dat` | SHA-1 of the whole pack |
| `t7_cll.zip` | SHA-1 of the archive |
| `s1.exe` | SHA-1 of the file |
| BOIII Community | SHA-1 from GitLab `updater.json` (`new version` in the UI) |

An older version on one item is skipped; the rest of the feed is still checked.

On startup a dialog lists pending updates (unless disabled). **Don't notify me about updates** turns auto-check off. Settings has the same toggle plus **Check for updates**.

The launcher can replace itself: it downloads a new exe, starts a helper process, exits, the helper swaps the file and relaunches.

Regenerate the feed:

```bat
python scripts\gen_cll_update.py -o cll_update.json
```

Hashes in the JSON use the field `hash` (SHA-1). Archives list child files the same way.

## Sidebar

- Home icon is larger; Mods / Server / Settings / About stay compact
- Pencil next to **GAMES** enters organize mode (dimmed UI, drag handles, first-time tip with OK)
- Order is saved in `LanLauncher.ini`

## How to use

1. Download the exe from **Releases**
2. Put it in a writable folder
3. First launch: language, theme, Portable kit or existing Plutonium, games you own
4. Open a game tab and click **Start**

LAN: in-game press `` ` `` and type `connect IP:port`.

## Building

Qt 6 (Widgets, Network, Concurrent, Svg) and CMake 3.16+.

Static build (example prefix `Y:\QT\6.11.2-static`):

```bat
scripts\build-app.bat
```

Output: `dist-static\CodLanLaucher.exe`.

Dynamic:

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.11.2\mingw_64
cmake --build build --config Release
```

Bump the release version only in `src/Version.h`.

## Translations

JSON in `resources/i18n/` is embedded via `resources.qrc`.

- Keys are the Portuguese `tr()` source strings
- `en.json`, `es.json`, `ru.json` translate them
- `pt_BR.json` mostly fixes accents

To add a language: copy `en.json`, translate, add it to the `.qrc`, register it in `I18n::codes()`.

## Portable kit

`pack_pu.py` builds `pu.dat` (not shipped inside the exe):

```bat
python pack_pu.py
```

Format: magic `LLQTPKG1` + size + XOR'd 7z.

## Structure

```
src/                 application code
src/pages/           Home, Play, Mods, Server, Settings, About
src/Version.h        CLL_VERSION
src/UpdateService.*  cll_update.json client
resources/           icons, art, themes, i18n
scripts/             static build + gen_cll_update.py
```

## Credits

- **MestreTM** — Cod Lan Launcher
- **[JugAndDoubleTap](https://github.com/JugAndDoubleTap/LanLauncher)** — original Python LanLauncher
- **[xerxes-at](https://github.com/xerxes-at)** — T4 / T5 / T6 dedicated server configs
- **[alterware.dev](https://alterware.dev)** — source for some standalone clients
- **[boiii-community](https://gitlab.com/boiii-community/BOIII-Community)** — alternative BO3 client
- **[ezz.lol](https://ezz.lol)** — native BO3 client source
- **TehTurkishSpartan** — Turkish translation (external)
- **Plutonium** and **Call of Duty** belong to their owners. This project is not affiliated with them.

## License

LGPL-3.0. See [LICENSE](LICENSE).
