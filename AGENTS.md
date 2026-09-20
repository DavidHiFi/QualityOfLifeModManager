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
src/GameLauncher.*   LAN and online launch (both the bootstrapper), ReShade watchdog
src/PlutoniumAuth.*  Plutonium account: saved token, validate, mint a session token
src/LoginDialog.*    sign-in box, shown only when online play has no account
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

**Online launch signs itself and starts the bootstrapper. No launcher, no
handler.** The bootstrapper has no login of its own: it wants a session token as
`-token <16 hex>` and without one it dies on `Could not authenticate to
Plutonium: non-success status code: 401`. Minting that token is the only thing
the official launcher was doing for us, so `PlutoniumAuth` does it instead:

* `GET /api/auth/validate` and `POST /api/auth/session` on `nix.plutonium.pw`,
  authorised with `Authorization: UserToken <user token>`.
* The user token is the account's, stored DPAPI-encrypted (current user) in
  `<plutonium>\config.json` under `token` — the same file, key and format the
  official launcher uses, so a login in either place works in both.
* `PlutoniumAuth::tokenRoot()` reads the configured install first and falls back
  to `%LOCALAPPDATA%\Plutonium`, so a portable kit still finds an existing login.
* `LoginDialog` appears only when there is no usable account, and the launch
  continues by itself afterwards.

**The API user agent must stay exactly `Nix/3.0`.** The service is behind a
filter that 403s a challenge page at anything else — `nix/3.0` and `Nix/3.1`
included, and our own user agent too. It is the protocol's client identity, not
a preference. Change it and every online launch is a 401 again.

A mod still cannot be pre-loaded online; `fs_game` is LAN only. Any Plutonium
root with a bootstrapper works now, registered or not.

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

`-selftest` exercises ReShade install and remove, watchdog unpack and start and
every theme, one PASS/FAIL line each, and reports the signed-in Plutonium
account as an INFO line (never a failure: a throwaway root has none).
Screenshots without a display: set `QOL_SCREENSHOT=<png>` and `QOL_PAGE=<index>`
and run the exe.

To prove a real online launch without touching the GUI:

```powershell
dist\QualityOfLifeModManager.exe -nogui -online -gameid T6 -mode ZM `
    -plutoniumdir "$env:LOCALAPPDATA\Plutonium" -gamedir "<Black Ops II folder>"
```

It prints the pid it started. A pass is a `Plutonium T6 Zombies (rNNNN)` window
whose parent is the app, with no `plutonium-launcher-win32.exe` in the tree.

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
  this build. LAN argument construction is the upstream's. Online launch was
  proven live on 2026-09-20 for T6 Zombies (window up, no launcher process, no
  401); T4, T5 and IW5 online use the identical code path with a different mode
  id but have not been booted.
