# AGENTS.md

Context for anyone - human or agent - picking this repository up cold.

## What this is

A mod manager for [Plutonium](https://plutonium.pw): install, update, launch and remove mods for
World at War, Black Ops and Black Ops II. C# WinForms, `net8.0-windows`, self-contained single
file. It is **not** the mod - the Black Ops II mod lives in
[T6-QoL](https://github.com/DavidHiFi/T6-QoL) and has its own release line.

## The things that are easy to break by accident

**One binary is three programs.** `Program.Main` reads its own file name: anything ending in
`Setup` opens the install window, `--uninstall` opens the removal window Windows calls from Apps &
features, and anything else opens the app. That is why the setup download and the portable
download are the same size. Do not add a second project for the installer.

**Versioning is independent of the mod.** The mod is 2.16.x; this is 1.0.x. The app compares its
own assembly version against *this* repository's latest release to offer a self-update, and the
mod against T6-QoL's. Conflating the two is what shipped "App update v2.15.51" with no package
behind it.

**`payload/` and `catalog.json` are embedded at build time** by the `PackPayload` target and
unpacked or read at runtime. `MakeDir` runs before `ZipDirectory` because `obj\` does not exist on
a clean clone.

**Pages own their scrolling.** Do not go back to `AutoScroll` plus `ShowScrollBar` to hide the
native bar: Windows still reserves its width, so rows get laid out to one width and painted at
another and the text smears. The mouse wheel is routed by hit test through an `IMessageFilter`
because nothing on these pages takes focus.

**The texture and sound packs are found by keyword, not by file name.** The texture pack has
shipped as both `zm_qol-textures.zip` and `HD.Texture.Pack.zip`, and its contents sit under
`HD Texture Pack/images/`. An exact-name lookup is why "Install everything" failed on its second
step; copying from the wrong root is how the files end up one folder too deep, where the game never
reads them. `PackRoot` picks the folder whose contents belong at the destination.

**Mod packages come in three shapes** - the mod folder's contents at the zip root, one folder
holding them, or a whole `Plutonium\storage\<game>\mods\<name>` tree. All three are handled in
`InstallModFromFileAsync`. The third is what Octagonal Ascension ships, and mishandling it
installed a folder called "pack" with no mod anywhere the game looks.

**A game page is the game; a mod gets its own page.** `ShowT6` is Black Ops II - playing, the mods
folder, ReShade. What the Quality of Life mod installs lives in `ShowQol` one level down, and
`ShowController` sits under that. Putting the texture pack and the sound pack back on the game page
makes them look like parts of the game rather than parts of one mod.

**One app window per Plutonium folder.** `SingleInstance` holds a named mutex keyed on a hash of
the resolved root; a second copy finds it taken and offers to bring the first window back through a
named event. The key has to include the root or a `--root` test run fights whatever the user has
open. Minimising to the tray *hides* the window and leaves the process alive - that is the case
that shipped two copies before, and any change here has to be checked against it, not just against
a visible window.

**Fonts are resolved, not assumed.** A machine-wide `FontSubstitutes` entry can redirect
"Segoe UI" to something else - GDI honours it while GDI+ does not, so the family reports as Segoe
UI while every label draws in the substitute. `Ui.ResolveFamily` picks one that survives the round
trip. Never fix this by editing the user's registry.

## Layout

```
src/            the app - one project, builds the whole thing
payload/        icon, launch scripts, ReShade presets and DLL; embedded in the exe
catalog.json    the browsable community mod list; embedded, and refreshed from this file on GitHub
src/package.ps1 builds both release downloads into dist/
```

## Building and releasing

```powershell
dotnet publish src/QolSeriesInstaller.csproj -c Release -r win-x64 --self-contained true
pwsh src/package.ps1        # -> dist/QualityOfLifeModManagerSetup.exe and the portable zip
```

Release with both files attached, tagged `vX.Y.Z` matching `<Version>` in the csproj. Users on an
older build self-update from the portable zip, so it must always be attached.

## Testing

There is no unit test project; the app is verified by driving the real thing. The scripts live
outside this repository, in the working folder used to build it - ask the user for
`modding-jobs\qol-portable-installer-001\verify\` if you need them. What they cover: update checks,
settings persistence, install/uninstall round trip, self-update, browse-and-install, the real mods
folders, scroll artifacts, the single-instance guard, and a visual sweep across themes.

Three traps that cost real time, worth knowing before writing another one:

- **`PrintWindow` cannot see scroll artifacts.** It re-renders the window from scratch and paints
  over the very thing you are looking for. Use `CopyFromScreen` for anything about what is
  actually on screen.
- **`SendMessage` bypasses `IMessageFilter`.** Use `PostMessage` to test the mouse wheel.
- **A modal file dialog blocks UIA entirely** - calls against the app time out, and `Invoke()` on
  the button that opened it does not return until the dialog closes. Drive those dialogs with
  window messages, or use `--export-settings` / `--import-settings`, which exist partly for this.

GitHub allows 60 unauthenticated API calls an hour. Running the suites back to back exhausts it,
and every check then fails with an empty tag - that is the rate limit, not a regression.

## Command line

```
--status-json              what is installed, as JSON
--export-settings <file>   app options plus the mod's in-game config
--import-settings <file>   restore them; add --app-only to skip the mod's config
--setup / --uninstall      the install and removal windows
--root <path>              use a different Plutonium folder (use this for any testing)
--allow-multiple           skip the "already open" question and start another copy
--ignore-running           testing only, and only with --root: install even though Plutonium is up
```

Always test against `--root` pointing at a throwaway folder. The app writes to a real Plutonium
install otherwise.
