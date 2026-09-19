# AGENTS.md

Context for anyone, human or agent, picking this repository up cold.

## What this is

Quality of Life Mod Manager 2.x: a Qt 6 / C++17 desktop app that installs, updates,
launches and removes Call of Duty mods on Plutonium, built around the Quality of Life
series. It is a fork of [Cod Lan Launcher](https://github.com/MestreTM/CLL-Cod-Lan-Launcher)
v1.2.0 (LGPL-3.0). The 1.x line was a C# WinForms app in this same repository; it was
retired on 2026-09-19 and its `main` history before the fork commit is that app.

It is not the mod. The Black Ops II mod lives in `DavidHiFi/T6-QoL` and has its own
release line (2.16.x); this app is 2.0.x. Do not compare the two.

## Layout

```
src/                 app code (upstream layout kept so upstream merges stay possible)
src/Version.h        version and every brand string, URL and feed address
src/QolService.*     the series: mod state, packs, controller icons, ReShade install
src/pages/QolPage.*  the "Quality of Life" sidebar page
src/GameLauncher.*   LAN launch (bootstrapper), online launch (plutonium://), watchdog
src/Theme.cpp        kThemes table: 3 upstream themes + 12 ported from the 1.x app
resources/reshade/   dxgi.dll and the presets, embedded, written to <pluto>\bin
resources/tools/     reshade-watchdog.ps1, embedded, unpacked to <exe>\tools\
qol_home.json        Home catalog (embedded fallback + fetched from raw GitHub)
qol_update.json      update feed; only the "launcher" item is ours
scripts/build.ps1    the release build (dynamic Qt + windeployqt into dist\)
```

## Things that are easy to break

**Brand and URLs live in `src/Version.h` only.** The `CLL_*` macro names are kept on
purpose so upstream diffs apply; their values are ours. Grep before adding a literal.

**Translation keys are the Portuguese source strings.** Upstream `tr("...")` literals
are Portuguese and `resources/i18n/*.json` map them. Any string we add is English
source text and simply has no translation, which is fine. Renaming an upstream
literal breaks its translation in four files.

**Online launch is the `plutonium://play/<id>` handler, never the bootstrapper.**
The bootstrapper started directly has no login and answers 401. `launchOnline()`
refuses when the configured Plutonium folder is not `%LOCALAPPDATA%\Plutonium`,
because the handler always starts the registered install. A mod cannot be
pre-loaded online; `fs_game` is LAN only.

**Mode ids for T4 and T5 Zombies are `t4sp` and `t5sp`.** `t4zm`/`t5zm` do not
exist; the bootstrapper prints usage and quits, which looks like a flash and no game.

**The ReShade vault path is shared with the watchdog script.** `QolService::reShadeVault`
and `$VaultDir` in `resources/tools/reshade-watchdog.ps1` must agree
(`storage\t6\_zm_qol_installer\reshade-vault`) or the watchdog has nothing to restore.
The watchdog never overwrites a file that exists in `bin`; it only puts back what
Plutonium deleted, and copies edited top-level `*.ini` back into the vault.

**Page indices.** `MainWindow::ToolIndex` must match the `m_stack->addWidget` order,
and sidebar tool buttons map with `kFirstTool`. Insert new pages in both places.

**windres.** On some machines the piped preprocessor fails with "when writing output
to : Invalid argument". `scripts/build.ps1` passes `--use-temp-file`. Keep it.

**Never write to a real Plutonium root while testing.** Use `-plutoniumdir` with a
throwaway folder that has an empty `bin\plutonium-bootstrapper-win32.exe`.

## Building

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Clean
dist\QualityOfLifeModManager.exe -selftest -plutoniumdir <throwaway root>
```

`-selftest` exercises ReShade install and remove, watchdog unpack and start, the
handler probe and every theme, one PASS/FAIL line each. Screenshots without a display:
set `QOL_SCREENSHOT=<png>` and `QOL_PAGE=<index>` and run the exe.

## Releasing

1. Bump `CLL_VERSION` in `src/Version.h`.
2. Build, zip `dist\` as `QualityOfLifeModManager-portable.zip`, and attach both the
   zip and the bare `QualityOfLifeModManager.exe` to a GitHub release tagged `vX.Y.Z`.
3. `python scripts\gen_update.py -o qol_update.json`, commit, push. The app compares
   its version to the `launcher` item and offers the exe.

## What is not done

* The 1.x app's backups page (per-kind backup and restore) has no equivalent yet;
  installs still take the one-time ReShade backup and manifests for removal.
* Zombies Declassified installs through its own manifest via the Mods page; there is
  no in-app verifier for its 9 GB payload.
* Live game testing of every launch path on a real install has not been done from
  this build. The handler probe and LAN argument construction are the upstream's.
