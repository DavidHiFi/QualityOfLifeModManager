#!/usr/bin/env python3
"""Build the versioned DLSS 5 LAN payload for Quality of Life Mod Manager.

This is a release-maintainer command. App users press Install once and never
choose a folder; the app downloads what this script publishes.

What goes in, and where it comes from:

  DLSS5-Feeder 0.15.1       jlrouzies-fr/DLSS5-Feeder release zip, pinned by
                            sha256 (MIT). The 32-bit add-on, its shader, the
                            64-bit helper.
  ReShade.fxh, ReShadeUI.fxh crosire/reshade-shaders at a pinned commit (CC0).
                            The feeder's shader and the Lumenite kernel both
                            include ReShade.fxh; a PC without a shader pack has
                            none, and the effect then fails to compile.
  nvngx_dlssnr.dll          NVIDIA's neural-rendering runtime. NVIDIA does not
                            publish it in a public repository, so it is taken
                            from the verified local install and pinned by hash.
  renodx-dlss5.addon64      the neural consumer DLSS 5 Swapper ships (MIT), pinned
                            by hash; not published as a release anywhere.
  host64\\d3d12.dll          ReShade 6.8.0 64-bit with full add-on support
                            (BSD-3), for the helper, pinned by hash. Named
                            d3d12.dll so a tool that preloads Windows' own
                            dxgi.dll into every process cannot shadow it.
  msvcp140 / vcruntime140   Microsoft's x64 VC++ redistributable. The helper
                            imports them and a PC that never installed a 64-bit
                            VC++ runtime cannot start it. Microsoft allows these
                            to sit beside the program that uses them.

What does NOT go in, and is downloaded by the app from its publisher instead:

  LumeniteFX                Its AGNYA licence forbids re-hosting ("independently
                            hosting a copy of this project and propagating it
                            using this hosted version is prohibited"), so it
                            comes from the author's repository at a pinned
                            commit, exactly as DLSS 5 Swapper does it.
  nvngx_dlss.dll            NVIDIA/DLSS at a pinned commit, the SDK's own
                            redistributable runtime, from NVIDIA's repository.

The payload records each one's URL and hash; the app accepts no other origin.

Every file is pinned. A donor file that has drifted stops the build rather
than publishing something nobody has tested.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import urllib.request
import zipfile


REPO = "DavidHiFi/QualityOfLifeModManager"
TAG = "dlss5-stable"

FEEDER_ZIP = ("https://github.com/jlrouzies-fr/DLSS5-Feeder/releases/download/v0.15.1/"
              "DLSS5-Feeder-0.15.1.zip",
              "2e44e81e691e75e532b9b7babc278a12615cb7f0fd9ef854da50e6ef17b272f4")
# payload path <- path inside the Feeder release zip
FROM_FEEDER = {
    "dlss5-feed.addon32": "dlss5-feed.addon32",
    "reshade-shaders/Shaders/DLSS5_Feed.fx": "reshade-shaders/Shaders/DLSS5_Feed.fx",
    "host64/dlss5-feed-host64.exe": "host64/dlss5-feed-host64.exe",
}

RESHADE_SHADERS = "https://raw.githubusercontent.com/crosire/reshade-shaders/ee30868391d4ad103db60489820102d8fd40e3c1/"
NVIDIA_DLSS = "https://raw.githubusercontent.com/NVIDIA/DLSS/374959484e79a640feaba44c93ac8cfb0a03f5b5/"
FROM_URL = {
    "reshade-shaders/Shaders/ReShade.fxh":
        (RESHADE_SHADERS + "Shaders/ReShade.fxh",
         "6dabfbbaf968c3871905d2ea17f96572ff7b1cec01310b5d0e5252b66b30174f"),
    "reshade-shaders/Shaders/ReShadeUI.fxh":
        (RESHADE_SHADERS + "Shaders/ReShadeUI.fxh",
         "78adf672df47460297eb9fe6dd238d2aafa24510b52b84feb1a745dff70eb901"),
    "host64/licenses/NVIDIA-RTX-SDK-LICENSE.txt":
        (NVIDIA_DLSS + "LICENSE.txt", None),
    "host64/licenses/DLSS5-Feeder-LICENSE.txt":
        ("https://raw.githubusercontent.com/jlrouzies-fr/DLSS5-Feeder/v0.15.1/LICENSE",
         "6562d5a5e3d7534711e34f4b34335f23f067acc839ae5274c1250bf5f4654b8b"),
    "host64/licenses/RenoDX-LICENSE.txt":
        ("https://raw.githubusercontent.com/clshortfuse/renodx/9b212edad4dde9bca2b823b1e045b712b1a8d854/LICENSE",
         "e9e2da3991a48d9c09b710c9ddbf5673275b6d569c976bcffab3111d0c509d51"),
    "host64/licenses/ReShade-LICENSE.txt":
        ("https://raw.githubusercontent.com/crosire/reshade/v6.8.0/LICENSE.md",
         "237ded5b8344f820113efab1e65e91e1f159d9202c5b4856606a0590d3ffdab0"),
}

# payload path <- path under --donor (a working DLSS5-Feeder install)
FROM_DONOR = {
    "host64/nvngx_dlssnr.dll": ("host64/nvngx_dlssnr.dll",
        "8270b350cd82de5ce89806872cdd6b6a9249b80836b91bbeb3573470744cc206"),
    "host64/renodx-dlss5.addon64": ("host64/renodx-dlss5.addon64",
        "d5adf82eb44b065f4c590ac91fe824bab07afea0eb9f994bde936710c8593952"),
    # Shipped as d3d12.dll, not dxgi.dll. The helper does LoadLibrary("dxgi.dll")
    # and then LoadLibrary("d3d12.dll"). Anything that has already pulled Windows'
    # own dxgi.dll into every process wins the first call. A Windhawk UI mod
    # scoped to "*" does this, so ReShade never loaded, the consumer never
    # attached and DLSS delivered nothing. Nothing loads d3d12.dll that early,
    # and ReShade under that name still hooks DXGI and D3D12.
    "host64/d3d12.dll": ("host64/dxgi.dll",
        "0cee63f9c9f13f3ac909c5b4903f4dbb4b719a7ab3b4f13b0deaf83c814b94f7"),
}

# payload path <- file name under --vcredist (VC\Redist\MSVC\<ver>\x64\Microsoft.VC143.CRT)
FROM_VCREDIST = ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")

# Fetched by the app from their publishers, never uploaded by us: LumeniteFX
# (the kernel makes the motion vectors the feeder reads, DLSS5_MV_PROVIDER=3,
# and the four includes are what it #includes) and NVIDIA's DLSS runtime from
# the DLSS SDK repository. The app accepts only these two origins.
LUMENITE_BASE = "https://raw.githubusercontent.com/umar-afzaal/LumeniteFX/f8cbbb4eccfcb7adf0d74bb358ba349272e3c1e9/"
REMOTE = {
    "reshade-shaders/Shaders/LumeniteFX/lumenite_Kernel.fx": LUMENITE_BASE + "Shaders/lumenite_Kernel.fx",
    "reshade-shaders/Shaders/LumeniteFX/include/lumenite_Projections.fxh": LUMENITE_BASE + "Shaders/include/lumenite_Projections.fxh",
    "reshade-shaders/Shaders/LumeniteFX/include/lumenite_Helpers.fxh": LUMENITE_BASE + "Shaders/include/lumenite_Helpers.fxh",
    "reshade-shaders/Shaders/LumeniteFX/include/lumenite_Compute.fxh": LUMENITE_BASE + "Shaders/include/lumenite_Compute.fxh",
    "reshade-shaders/Shaders/LumeniteFX/include/lumenite_ColorManagement.fxh": LUMENITE_BASE + "Shaders/include/lumenite_ColorManagement.fxh",
    "host64/licenses/LumeniteFX-LICENSE.md": LUMENITE_BASE + "LICENSE.md",
    "host64/licenses/LumeniteFX-NOTICE": LUMENITE_BASE + "NOTICE",
    "host64/nvngx_dlss.dll": NVIDIA_DLSS + "lib/Windows_x86_64/rel/nvngx_dlss.dll",
}
REMOTE_PINS = {
    "reshade-shaders/Shaders/LumeniteFX/lumenite_Kernel.fx":
        "dc44d101c568a8492606884037c86059a31b844fd5e144e733fb70dabc91f25c",
    "host64/nvngx_dlss.dll": "3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983",
}

# The feeder starts off: the app turns it on for a LAN session that asked for
# DLSS. host_window=0 keeps the helper's panel out of the game until F10; its
# redraw costs ~28 ms of CPU per frame while it is shown.
FEEDER_CONFIG = (b"enabled=0\r\nmode=2\r\nhdr=-1\r\ndepth_inverted=-1\r\nflags=-1\r\n"
                 b"reset_every=0\r\nwarmup_rebuild=180\r\nrebuild=0\r\nlog_frames=3\r\n"
                 b"create_delay=60\r\npreset=0\r\nwork_resolution=100\r\nmv_scale_x=1.000\r\n"
                 b"mv_scale_y=1.000\r\nhost_window=0\r\nasync_home=1\r\n")

# The helper's own ReShade. The panel it draws is cast into the game, so its
# windows can only move inside that picture. WindowWidth 1500 gives them room
# (900 pinned the DLSS 5 Swapper page to 178 px), and TutorialProgress=4 keeps
# ReShade's first-run banner off the top of it. No Docking=/Window= lines: the
# helper writes a fresh layout for the width it is given.
HOST_CONFIG = (b"[ADDON]\r\nAddonPath=.\\\r\n\r\n"
               b"[DLSS5Host]\r\nWindowWidth=1500\r\nWindowHeight=0\r\n\r\n"
               b"[OVERLAY]\r\nTutorialProgress=4\r\n\r\n"
               b"[RenoDX.DLSS5]\r\nEnableHooks=2\r\nNeuralUplift=1\r\nNREnableUpscaling=0\r\n"
               b"NRToggleKey=0\r\nNRScreenshotKey=0\r\n")

NOTICE = b"""DLSS 5 LAN payload for Quality of Life Mod Manager
=================================================

Installed by the manager into Plutonium's storage and copied into Plutonium's
bin folder only for LAN launches of Black Ops II. Online launches remove it.

  dlss5-feed.addon32, DLSS5_Feed.fx,   DLSS5-Feeder 0.15.1 by Jean-Laurent Rouzies
  host64\\dlss5-feed-host64.exe        https://github.com/jlrouzies-fr/DLSS5-Feeder (MIT)
  host64\\renodx-dlss5.addon64         RenoDX DLSS 5 consumer by Carlos Lopez Jr.
                                       https://github.com/clshortfuse/renodx (MIT)
  host64\\d3d12.dll                    ReShade 6.8.0 by Patrick Mours, https://reshade.me (BSD-3-Clause)
  reshade-shaders\\Shaders\\ReShade*.fxh  crosire/reshade-shaders (CC0)
  host64\\nvngx_dlssnr.dll             NVIDIA neural rendering runtime, (c) NVIDIA
                                       Corporation, under the NVIDIA RTX SDKs licence
  host64\\msvcp140.dll, vcruntime140*  Microsoft Visual C++ runtime (redistributable)

Two things are not in this archive. The manager downloads them from their
publishers at the commits named in payload.json and checks each file against
the hash recorded there:

  LumeniteFX (motion-vector kernel)   https://github.com/umar-afzaal/LumeniteFX
                                      Its licence requires the author's own links.
  host64\\nvngx_dlss.dll               https://github.com/NVIDIA/DLSS
"""


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fetch(url, want=None):
    req = urllib.request.Request(url, headers={"User-Agent": "QualityOfLifeModManager-payload"})
    with urllib.request.urlopen(req, timeout=300) as r:
        data = r.read()
    if want and digest(data) != want:
        raise SystemExit(f"{url} does not match its pinned hash ({digest(data)})")
    if not data:
        raise SystemExit(f"{url} came back empty")
    return data


def build(donor, vcredist, output, version):
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+){2}", version):
        raise SystemExit("version must be three dotted numbers, such as 1.1.0")
    output.mkdir(parents=True, exist_ok=True)
    files = {}

    feeder = zipfile.ZipFile(io.BytesIO(fetch(*FEEDER_ZIP)))
    for name, inner in FROM_FEEDER.items():
        files[name] = feeder.read(inner)
    for name, (url, want) in FROM_URL.items():
        files[name] = fetch(url, want)
    for name, (rel, want) in FROM_DONOR.items():
        path = donor / rel
        data = path.read_bytes() if path.is_file() else b""
        if digest(data) != want:
            raise SystemExit(f"{path} is missing or not the pinned build")
        files[name] = data
    for dll in FROM_VCREDIST:
        path = vcredist / dll
        if not path.is_file():
            raise SystemExit(f"missing VC++ runtime: {path}")
        files[f"host64/{dll}"] = path.read_bytes()
    files["dlss5-feed.cfg"] = FEEDER_CONFIG
    files["host64/ReShade.ini"] = HOST_CONFIG
    files["host64/licenses/NOTICE.txt"] = NOTICE

    # The app downloads these itself; record exactly what they must be.
    remote = {}
    for name, url in REMOTE.items():
        data = fetch(url, REMOTE_PINS.get(name))
        remote[name] = {"url": url, "size": len(data), "sha256": digest(data)}

    payload = {
        "version": version,
        "files": {name: {"size": len(data), "sha256": digest(data)}
                  for name, data in sorted(files.items())},
        "remote": remote,
    }
    archive = output / f"dlss5-payload-v{version}.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6, allowZip64=True) as z:
        for name, data in sorted(files.items()):
            z.writestr(name, data)
        z.writestr("payload.json", json.dumps(payload, indent=2, sort_keys=True) + "\n")
    with zipfile.ZipFile(archive) as z:
        bad = z.testzip()
        if bad:
            raise SystemExit(f"archive CRC failed: {bad}")

    archive_hash = hashlib.sha256()
    with archive.open("rb") as src:
        for block in iter(lambda: src.read(4 * 1024 * 1024), b""):
            archive_hash.update(block)
    manifest = {
        "version": version,
        "url": f"https://github.com/{REPO}/releases/download/{TAG}/{archive.name}",
        "size": archive.stat().st_size,
        "sha256": archive_hash.hexdigest(),
    }
    (output / "dlss5-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"{archive} ({archive.stat().st_size} bytes, SHA-256 {manifest['sha256']})")
    print(f"{len(files)} files in the archive, {len(remote)} fetched by the app from their publishers")
    print(output / "dlss5-manifest.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--donor", type=Path, required=True,
                        help="game folder with a working DLSS5-Feeder install (for host64)")
    parser.add_argument("--vcredist", type=Path, required=True,
                        help=r"...\VC\Redist\MSVC\<ver>\x64\Microsoft.VC143.CRT")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    build(args.donor, args.vcredist, args.output, args.version)
