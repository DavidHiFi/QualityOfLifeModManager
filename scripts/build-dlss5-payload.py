#!/usr/bin/env python3
"""Build the versioned DLSS 5 LAN asset from an already verified local install.

This is a release-maintainer command. App users download the resulting asset
with one click and never choose a donor folder.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile


REPO = "DavidHiFi/QualityOfLifeModManager"
TAG = "dlss5-stable"
SOURCES = {
    "dlss5-feed.addon32": "dlss5-feed.addon32",
    "reshade-shaders/Shaders/DLSS5_Feed.fx": "reshade-shaders/Shaders/DLSS5_Feed.fx",
    "reshade-shaders/Shaders/LumeniteFX/lumenite_Kernel.fx": "reshade-shaders/Shaders/lumenite_Kernel.fx",
    "host64/dlss5-feed-host64.exe": "host64/dlss5-feed-host64.exe",
    "host64/dxgi.dll": "host64/dxgi.dll",
    "host64/renodx-dlss5.addon64": "host64/renodx-dlss5.addon64",
    "host64/nvngx_dlss.dll": "host64/nvngx_dlss.dll",
    "host64/nvngx_dlssnr.dll": "host64/nvngx_dlssnr.dll",
}
# The two Feeder binaries and its shader must come from one release. The
# consumer is the same verified build used by DLSS5-Swapper's feeder route.
PINS = {
    "dlss5-feed.addon32": "fb69357075cba536b42f2e693992fde0c1775058b1fd9f10fbdf1e7eba3443e7",
    "reshade-shaders/Shaders/DLSS5_Feed.fx": "cdac08a721b14b97187dd86c5b5bead157c9063d7ee859a0f131a8ee791695f1",
    "host64/dlss5-feed-host64.exe": "034e6cdf382e6ea9164afd63c5b8a8aa0312894cb342593bc933e9fa435f8d02",
    "host64/renodx-dlss5.addon64": "d5adf82eb44b065f4c590ac91fe824bab07afea0eb9f994bde936710c8593952",
}
FEEDER_CONFIG = b"enabled=0\r\nmode=2\r\nhdr=-1\r\ndepth_inverted=-1\r\nflags=-1\r\nreset_every=0\r\nwarmup_rebuild=180\r\nrebuild=0\r\nlog_frames=3\r\ncreate_delay=60\r\npreset=0\r\nwork_resolution=100\r\nmv_scale_x=1.000\r\nmv_scale_y=1.000\r\nhost_window=0\r\nasync_home=1\r\n"
HOST_CONFIG = b"[ADDON]\r\nAddonPath=.\\\r\n\r\n[RenoDX.DLSS5]\r\nEnableHooks=2\r\nNeuralUplift=1\r\nNREnableUpscaling=0\r\nNRToggleKey=0\r\n"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def build(donor, output, version):
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+){2}", version):
        raise SystemExit("version must be three dotted numbers, such as 1.0.0")
    output.mkdir(parents=True, exist_ok=True)
    files = {}
    for name, source in SOURCES.items():
        path = donor / source
        if not path.is_file():
            raise SystemExit(f"missing source: {path}")
        data = path.read_bytes()
        if not data:
            raise SystemExit(f"empty source: {path}")
        if name in PINS and digest(data) != PINS[name]:
            raise SystemExit(f"{name} does not match the pinned Feeder/consumer release")
        files[name] = data
    files["dlss5-feed.cfg"] = FEEDER_CONFIG
    files["host64/ReShade.ini"] = HOST_CONFIG

    payload = {
        "version": version,
        "files": {name: {"size": len(data), "sha256": digest(data)}
                  for name, data in sorted(files.items())},
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
    print(output / "dlss5-manifest.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--donor", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    build(args.donor, args.output, args.version)
