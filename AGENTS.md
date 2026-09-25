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
src/VersionCompare.h the one rule for "is there an update", shared by both callers
src/LoginDialog.*    sign-in box, shown only when online play has no account
src/Theme.cpp        kThemes table: 3 upstream themes + 12 ported from the 1.x app
resources/reshade/   dxgi.dll and the presets, embedded, written to <pluto>\bin
resources/tools/     reshade-watchdog.ps1, embedded, unpacked to <exe>\tools\
qol_home.json        Home catalog (embedded fallback + fetched from raw GitHub)
qol_update.json      update feed; only the "launcher" item is ours
scripts/build.ps1    the release build (dynamic Qt + windeployqt into dist\)
scripts/verify-runtime.ps1 checks PE imports and runs the installed runtime probe
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

**The update feed is a fallback, not the source of truth.** `qol_update.json`
is a file somebody regenerates with `scripts/gen_update.py`, so on its own it
pins the LAN clients to whatever version was current the last time anyone ran
that - T7-CLL and S1-CLL would sit on one release forever. `UpdateService::
resolveLatest()` therefore asks each component's own repository first, by
reading the redirect from
`https://github.com/<repo>/releases/latest/download/<asset>` to
`/releases/download/<tag>/<asset>`. One request, plain `github.com`, so no
api.github.com rate limit and no token - and **api.github.com is blocked on some
machines, this one included**, which is why the API is not used. Anything it
cannot reach keeps the feed's values, so offline degrades to the old behaviour.
It only asks about components that are actually installed. Check it with
`-checkupdates`, which prints `live` or `feed` per component.

**A checksum the app cannot use is not a checksum, and must never be able to
fail a download.** v2.2.1's feed carried a 39-character sha1 - the feed was
hand-edited because api.github.com would not resolve that day, and one
character was lost. `!hash.isEmpty()` was the only test, so it was enforced,
matched nothing, and every installed copy refused its own update with "SHA-1
mismatch for QualityOfLifeModManager.update.bin". The app could not recover on
its own; the feed had to be corrected. Now `UpdateService::normalizedDigest`
drops anything that is not exactly 40 (or 64) lowercase hex digits, warns, and
the download falls back to the next-strongest check - GitHub's sha256, then the
published size, then, for the launcher payload, that it is a real PE. Digests
and sizes are dropped *together* when a live lookup moves past the release the
feed described: a stale size fails a download exactly as wrongly as a stale
digest. `-selftest` covers all of this, and checks the live feed's digests too.

**Regenerate the feed, never edit it by hand.** `python scripts/gen_update.py`
refuses to write a malformed digest, and falls back to the release redirect
when api.github.com is unreachable, which is the situation that caused the
hand-edit. Before committing a feed, run
`python scripts/gen_update.py --verify qol_update.json`: it downloads every
item and checks the digest and size actually served.

**Never decide "there is an update" by comparing two version strings for
inequality.** Use `VersionCompare::isUpdate(have, latest)`, which only says yes
when both sides parse as a plain dotted version and `latest` is strictly ahead.
The strings come from three sloppy sources that do not agree: a GitHub release
tag (`beta2`, `v1.2`), a `mod.json` version an author typed (`^32.16.16`), and
the catalog's `version`. Comparing them raw is what made every Home card say
"Update" forever, including right after the user updated. A card that is briefly
quiet about a new release is a much smaller failure than one that cries wolf
permanently. `HomeCatalog::presenceOf` prefers the installed `mod.json` version,
because that is what the catalog quotes; the release tag is only a fallback.

**A bad download and a bad pack look identical and have opposite fixes.** The
optional packs are hundreds of megabytes each, and `downloadAndUnpack` used to
hand whatever arrived straight to 7-Zip. When `zm_qol-sounds.zip` turned out to
have one CRC-broken member inside it, every user who pressed Install was shown
raw `7-Zip failed (code 2) ... ERROR: CRC Failed : zone\zmb_common.english.sabs`
and had no way to tell that the fault was on the releases page, not on their PC.
It had been that way since v2.14.28. So: `findAsset` now carries GitHub's own
`digest` (`sha256:<hex>`) and `size` for the asset, the download is checked
against both, and a failure is retried **once**. If the download matches and the
unpack still fails, the pack itself is damaged - say exactly that, and do not
retry. Re-cut a damaged pack from `t6\mods\zm_qol\Optionals\`, confirm with
`7z t`, and `gh release upload --clobber` it to **every** release that carries
it; the app scans releases newest-first, so an old corrupt copy is still
reachable until it is replaced.

**A digest proves the transfer, not the contents.** The corrupt sounds pack
matched its published sha256 perfectly - the bytes were damaged before upload,
so GitHub hashed the damage. The only thing that catches that is testing the
archive, which is why a verified-but-unpackable download is reported as a
damaged pack rather than a network problem.

**Removing something must remove it, not just un-apply a manifest.**
`installed-<kind>.txt` lists what this app installed, and for texture and sound
packs that manifest *is* the installed-state test, so the two cannot disagree.
ReShade is different: `reShadeInstalled()` checks for `bin\dxgi.dll` on disk, so
when ReShade arrived any other way there was no manifest, Remove deleted
nothing, returned success, and the button still said Remove. `removeReShade`
now takes the whole `kReShadeFiles` set from `bin` regardless, stops the
watchdog first (it would put the files straight back), refuses while a game is
running rather than failing on a locked DLL, and re-checks the result before
reporting success.

**Mode ids for T4 and T5 Zombies are `t4sp` and `t5sp`.** `t4zm`/`t5zm` do not
exist; the bootstrapper prints usage and quits, which looks like a flash and no game.

**The ReShade vault path is shared with the watchdog script.** `QolService::reShadeVault`
and `$VaultDir` in `resources/tools/reshade-watchdog.ps1` must agree
(`storage\t6\_zm_qol_installer\reshade-vault`) or the watchdog has nothing to restore.
The watchdog never overwrites a file that exists in `bin`; it only puts back what
Plutonium deleted, and copies edited top-level `*.ini` back into the vault.

**DLSS 5 is a downloaded payload with its own release tag, and it never goes online.**
The app reads `dlss5-manifest.json` from the `dlss5-stable` release (`QOL_DLSS_RELEASE_URL`),
downloads the archive it names, checks the archive's size and sha256, then checks every
file against the archive's own `payload.json`. It unpacks with Windows' `tar.exe`, so
7-Zip is not needed, and swaps the result into `reshade-lan\` whole. Rules that were paid for:

* **Never re-host LumeniteFX.** Its AGNYA licence forbids it. The payload lists it
  under `remote` with the author's raw URL and a hash, and the app fetches it from
  there. `nvngx_dlss.dll` comes from `NVIDIA/DLSS` the same way. The app accepts no
  other `remote` origin (`kRemoteOrigins`). Payload 1.0.0 carried the kernel and was
  deleted from the release for that reason.
* **The payload must run on a PC with no shader pack.** 1.0.0 had no `ReShade.fxh`,
  no Lumenite includes and no 64-bit VC++ runtime. It worked only on this PC, which had
  all three from other installs. `kDlssRequired` and `-selftest -testdlss` now fail on that.
* **Online is enforced in `GameLauncher::launchOnline`, not in the GUI.** Both the
  button and `-nogui -online` go through it. It switches bin to the stock build, parks
  any stray `*.addon*` (DLSS 5 Swapper puts its own feeder in bin), and refuses to
  start unless `onlineReShadeSafe` passes.
* **A LAN session puts bin back when it ends** (`MainWindow::afterSessionEnded`,
  and again at app start). Otherwise the add-on build sits in bin between sessions, and
  Plutonium's own launcher would start an online game on top of it.
* **Publishing a new payload:** `python scripts\build-dlss5-payload.py --donor <game
  folder with a working feeder> --vcredist <...\x64\Microsoft.VC143.CRT> --output <dir>
  --version X.Y.Z`. Upload the zip, then `--clobber` the manifest last. Every file is
  pinned; a donor file that has drifted stops the build.

**Page indices.** `MainWindow::ToolIndex` must match the `m_stack->addWidget` order,
and sidebar tool buttons map with `kFirstTool`. Insert new pages in both places.

**windres.** On some machines the piped preprocessor fails with "when writing output
to : Invalid argument". `scripts/build.ps1` passes `--use-temp-file`. Keep it.

**Never write to a real Plutonium root while testing.** Use `-plutoniumdir` with a
throwaway folder that has an empty `bin\plutonium-bootstrapper-win32.exe`.

## Building

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Clean
dist\QualityOfLifeModManager.exe -installcheck
dist\QualityOfLifeModManager.exe -selftest -plutoniumdir <throwaway root>
dist\QualityOfLifeModManager.exe -selftest -testdlss -plutoniumdir <throwaway root>
```

`-testdlss` downloads the published DLSS 5 payload (about 180 MB plus LumeniteFX and
NVIDIA's runtime) into a scratch root, then checks LAN, online, end of session,
update in place and removal.

`-selftest` exercises ReShade install and remove, watchdog unpack and start,
every theme, and the Home card / QoL row update rules, one PASS/FAIL line each.
It reports the signed-in Plutonium account and what each catalog mod currently
resolves to as INFO lines (never failures: a throwaway root has neither). The
ReShade checks need no game running - `removeReShade` refuses while one is, on
purpose, because the DLL is loaded.
Screenshots without a display: set `QOL_SCREENSHOT=<png>` and `QOL_PAGE=<index>`
and run the exe.

To prove a real online launch without touching the GUI:

```powershell
dist\QualityOfLifeModManager.exe -nogui -online -gameid T6 -mode ZM `
    -plutoniumdir "$env:LOCALAPPDATA\Plutonium" -gamedir "<Black Ops II folder>"
```

It prints the pid it started. A pass is a `Plutonium T6 Zombies (rNNNN)` window
whose parent is the app, with no `plutonium-launcher-win32.exe` in the tree.

To see what the updater resolves, without the GUI:

```powershell
dist\QualityOfLifeModManager.exe -checkupdates
```

Every installed component should print `live`, with `feed` only for ones that
are not installed or have no GitHub release. It is a GUI-subsystem exe, so
redirect stdout to a file rather than reading it off the pipe.

## Releasing

1. Bump `CLL_VERSION` in `src/Version.h`.
2. `powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Clean`, then
   `powershell -ExecutionPolicy Bypass -File scripts\build-setup.ps1 -NsisDir H:\Plutonium\tools\w64devkit\share\nsis`,
   which verifies every imported DLL, checks a contained extraction of the finished
   installer, and writes `release\QualityOfLifeModManagerSetup.exe`,
   `release\QualityOfLifeModManager-portable.zip`, and
   `release\QualityOfLifeModManager.update.bin`. Attach those three files to a
   GitHub release tagged `vX.Y.Z`. Never attach a bare
   `QualityOfLifeModManager.exe`: users downloaded it as though it were Setup, and
   Windows then opened one missing-Qt dialog after another.

   The release notes lead with the Setup. Paste this at the top:

   ```markdown
   Download **QualityOfLifeModManagerSetup.exe** and run it - that is the app.
   The `.update.bin` asset is consumed by the app and is not an installer.
   ```
3. `python scripts\gen_update.py -o qol_update.json`, commit, push. The app compares
   its version to the `launcher` item and offers the `.update.bin` payload.

### Publishing when api.github.com will not resolve

This comes and goes. As of 2026-09-23 the API resolves here and plain `gh`
works, so try `gh` first and only fall back to the pinned addresses below if it
fails. When it *is* blocked, `gh auth status` reports the token as invalid,
which is misleading: `GH_TOKEN` is fine, the local resolver (10.64.0.1) just
hands back dead addresses for the API hosts. Pin known-good ones per call
instead - never edit DNS or the hosts file:

```
api.github.com     140.82.112.5     (.113.5, .114.5 also work)
uploads.github.com 4.237.22.36      (this is the real alambic origin; the
                                     140.82.112.9 front answers 403 Unicorn
                                     for asset uploads)
```

```bash
curl -sS --resolve api.github.com:443:140.82.112.5   -X POST -H "Authorization: Bearer $GH_TOKEN"   -H "Accept: application/vnd.github+json"   https://api.github.com/repos/DavidHiFi/QualityOfLifeModManager/releases   -d '{"tag_name":"vX.Y.Z","name":"...","body":"...","draft":false}'

curl -sS --resolve uploads.github.com:443:4.237.22.36   -X POST -H "Authorization: Bearer $GH_TOKEN"   -H "Content-Type: application/octet-stream" --data-binary @<file>   "https://uploads.github.com/repos/DavidHiFi/QualityOfLifeModManager/releases/<id>/assets?name=<file>"
```

Read the release back afterwards and compare every asset `size` against the
local file. A 201 is not proof on its own.

**NSIS lives at `H:\Plutonium\tools\w64devkit\share\nsis`**, not in
Program Files. Pass that root as `-NsisDir`; `makensis.exe` alone (the copy in
`w64devkit\bin`) fails with "error setting default stub" because it looks for
`Stubs\` beside itself.

## Self-update

`src/SelfUpdate.*` owns replacing the .exe, and it exists because every part of
that step fails quietly. Windows will not overwrite a running program, so the
swap must close the app's **other** copies first - matched by full image path,
so a portable copy elsewhere is left alone - and the exiting instance must
leave through `ExitProcess`, because one that stalls on the way out keeps the
write lock and poisons every later update on that machine.

Two rules worth keeping:

* **Never stamp a version you have not started.** `reconcile()` decides whether
  an update worked by comparing `CLL_VERSION` against the pending record, not
  by trusting a stamp the previous build wrote about itself.
* **Never relaunch as though a failed swap succeeded.** After two failures at
  one version the automatic check stops offering it and the app says why. That
  is the only thing standing between a locked file and an endless
  download-restart-offer loop.

The helper gets **native** separators. `cmd`'s `copy` tolerates forward slashes;
its `del` does not, which silently leaves a 12 MB staged build behind.

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
