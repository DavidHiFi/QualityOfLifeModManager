#!/usr/bin/env python3
"""Build qol_update.json from GitHub latest releases (and optional local files).

Output is consumed by the app from:
  https://raw.githubusercontent.com/DavidHiFi/QualityOfLifeModManager/main/qol_update.json

Archives are hashed as the parent; every unpacked file is listed as a child.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

UA = "QoL-update-gen/2.0"
HOSTED = "https://raw.githubusercontent.com/DavidHiFi/QualityOfLifeModManager/main/qol_update.json"

# id, repo, asset name on the latest release, kind
CATALOG = [
    {
        "id": "launcher",
        "repo": "DavidHiFi/QualityOfLifeModManager",
        # Keep the updater payload off the public release's list of runnable
        # .exe files. People repeatedly downloaded the old bare exe as though
        # it were Setup, then Windows showed one missing-Qt dialog per DLL.
        "asset": "QualityOfLifeModManager.update.bin",
        "kind": "file",
    },
    {
        "id": "pu",
        "repo": "MestreTM/CLL-Cod-Lan-Launcher",
        "asset": "pu.dat",
        "kind": "archive",
    },
    {
        "id": "t7-cll",
        "repo": "MestreTM/t7-cll",
        "asset": "t7_cll.zip",
        "kind": "archive",
    },
    {
        "id": "s1-cll",
        "repo": "MestreTM/s1-cll",
        "asset": "s1.exe",
        "kind": "file",
    },
    {
        "id": "boiii-community",
        "repo": "boiii-community/BOIII-Community",
        "asset": "boiii.exe",
        "kind": "updater",
        "updater": "https://gitlab.com/boiii-community/BOIII-Community/-/raw/main/updater.json?ref_type=heads",
    },
]


def latest_url(repo: str, asset: str) -> str:
    return f"https://github.com/{repo}/releases/latest/download/{asset}"


def tag_url(repo: str, tag: str, asset: str) -> str:
    return f"https://github.com/{repo}/releases/download/{tag}/{asset}"


def api_json(url: str):
    req = urllib.request.Request(
        url,
        headers={"User-Agent": UA, "Accept": "application/vnd.github+json"},
    )
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode("utf-8"))


def api_releases(repo: str) -> list[dict]:
    try:
        rows = api_json(f"https://api.github.com/repos/{repo}/releases?per_page=100")
    except Exception as exc:
        print(f"  api.github.com unreachable ({exc}); using the release redirect")
        return []
    return rows if isinstance(rows, list) else []


SHA1_RE = re.compile(r"^[0-9a-f]{40}$")


def check_digest(where: str, value: str) -> str:
    """Refuse to emit a digest the app cannot compare against.

    v2.2.1 shipped a 39-character sha1 - the feed had been hand-edited because
    api.github.com would not resolve here, and one character was lost. It could
    not match any file that has ever existed, so every installed copy failed its
    own update with "SHA-1 mismatch". Nothing checked, on either side.
    """
    v = (value or "").strip().lower()
    if not SHA1_RE.match(v):
        raise SystemExit(
            f"refusing to write the feed: {where} has a malformed sha1 "
            f"({len(v)} chars, expected 40 hex): {v!r}"
        )
    return v


def redirect_tag(repo: str, asset: str) -> str:
    """Latest release tag via the /releases/latest/download redirect.

    No api.github.com, so this still works when the API host is unreachable -
    which is the situation that led to the feed being edited by hand.
    """
    url = latest_url(repo, asset)

    class _Redirected(Exception):
        def __init__(self, url):
            self.url = url

    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            raise _Redirected(newurl)

    opener = urllib.request.build_opener(NoRedirect)
    try:
        opener.open(urllib.request.Request(url, headers={"User-Agent": UA}), timeout=30)
    except _Redirected as r:
        m = re.search(r"/releases/download/([^/]+)/", r.url)
        return urllib.parse.unquote(m.group(1)) if m else ""
    except Exception:
        return ""
    return ""


def release_asset(rel: dict, asset: str) -> dict | None:
    name = asset.lower()
    for a in rel.get("assets") or []:
        if str(a.get("name", "")).lower() == name:
            return a
    return None


def resolve_release(repo: str, asset: str) -> tuple[str, str, bool]:
    """Newest release that actually contains the asset.

    Returns (tag, download_url, is_latest_release).
    """
    releases = api_releases(repo)
    if not releases:
        # No API: the /releases/latest/download redirect still names the tag,
        # and it only exists if that release really carries the asset. This is
        # the path that stops anyone having to hand-edit the feed again.
        return redirect_tag(repo, asset), latest_url(repo, asset), True
    latest_tag = str(releases[0].get("tag_name") or "")
    for rel in releases:
        if rel.get("draft") or rel.get("prerelease"):
            continue
        found = release_asset(rel, asset)
        if not found:
            continue
        tag = str(rel.get("tag_name") or "")
        url = found.get("browser_download_url") or tag_url(repo, tag, asset)
        return tag, url, tag == latest_tag
    # last resort: include pre-releases
    for rel in releases:
        found = release_asset(rel, asset)
        if not found:
            continue
        tag = str(rel.get("tag_name") or "")
        url = found.get("browser_download_url") or tag_url(repo, tag, asset)
        return tag, url, tag == latest_tag
    return latest_tag, latest_url(repo, asset), True


def sha1_file(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=180) as r, dest.open("wb") as out:
        shutil.copyfileobj(r, out)


def download_with_fallback(urls: list[str], dest: Path) -> str:
    last_err = None
    seen = set()
    for url in urls:
        if not url or url in seen:
            continue
        seen.add(url)
        print(f"download {url}")
        try:
            download(url, dest)
            return url
        except urllib.error.HTTPError as exc:
            last_err = exc
            print(f"  {exc} — trying older tag")
    if last_err:
        raise last_err
    raise RuntimeError("no download URL")


def find_7z() -> str | None:
    for name in ("7z", "7z.exe", "7za"):
        p = shutil.which(name)
        if p:
            return p
    return None


def extract_archive(archive: Path, dest: Path) -> bool:
    dest.mkdir(parents=True, exist_ok=True)
    suffix = archive.suffix.lower()
    if suffix == ".zip" or zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
        return True
    seven = find_7z()
    if seven:
        r = subprocess.run(
            [seven, "x", str(archive), f"-o{dest}", "-y"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return r.returncode == 0
    return False


def child_entries(root: Path) -> list[dict]:
    out = []
    for p in sorted(root.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(root).as_posix()
        out.append({"path": rel, "hash": sha1_file(p), "size": p.stat().st_size})
    return out


def resolve_local(overrides: dict[str, Path], asset: str) -> Path | None:
    if asset in overrides:
        return overrides[asset]
    key = Path(asset).name
    return overrides.get(key)


def build_community(spec: dict) -> dict:
    raw = api_json(spec["updater"])
    for row in raw if isinstance(raw, list) else []:
        if not isinstance(row, list) or len(row) < 4:
            continue
        if str(row[0]).lower() != "boiii.exe":
            continue
        return {
            "id": spec["id"],
            "name": "boiii.exe",
            "repo": spec["repo"],
            "url": str(row[3]),
            "hash": str(row[2]).lower(),
            "size": int(row[1]),
        }
    raise RuntimeError("boiii.exe not found in community updater.json")


def build_item(spec: dict, cache: Path, overrides: dict[str, Path]) -> dict:
    if spec.get("kind") == "updater":
        print(f"read {spec.get('updater')}")
        return build_community(spec)

    repo = spec["repo"]
    asset = spec["asset"]
    tag, asset_url, is_latest = resolve_release(repo, asset)
    # Prefer the floating latest URL when the current release has the file.
    url = latest_url(repo, asset) if is_latest else asset_url

    local = resolve_local(overrides, asset)
    if local and local.is_file():
        src = local
    else:
        src = cache / spec["id"] / asset
        used = download_with_fallback(
            [url, asset_url, tag_url(repo, tag, asset) if tag else ""],
            src,
        )
        if not is_latest:
            url = used

    item = {
        "id": spec["id"],
        "name": asset,
        "repo": repo,
        "version": tag,
        "url": url,
        "hash": check_digest(f"{spec['id']} ({asset})", sha1_file(src)),
        "size": src.stat().st_size,
    }

    if spec["kind"] == "archive":
        with tempfile.TemporaryDirectory(prefix="cllupd_") as tmp:
            extracted = extract_archive(src, Path(tmp))
            if extracted:
                children = child_entries(Path(tmp))
                if children:
                    item["files"] = children
            else:
                print(f"warn: could not unpack {asset}", file=sys.stderr)

    return item


def parse_overrides(pairs: list[str]) -> dict[str, Path]:
    out: dict[str, Path] = {}
    for raw in pairs:
        if "=" not in raw:
            p = Path(raw)
            out[p.name] = p
            continue
        name, path = raw.split("=", 1)
        out[name.strip()] = Path(path.strip())
    return out


def verify_feed(path: Path, cache: Path) -> int:
    """Check a feed against what is actually published.

    Run this on the file you are about to commit. Every digest has to be
    well-formed, and every one that is well-formed has to match the bytes the
    URL serves - the two ways this file has been wrong.
    """
    doc = json.loads(path.read_text(encoding="utf-8"))
    bad = 0
    for item in doc.get("items", []):
        ident = item.get("id", "?")
        h = (item.get("hash") or "").strip().lower()
        if not SHA1_RE.match(h):
            print(f"FAIL {ident}: malformed sha1 ({len(h)} chars): {h!r}")
            bad += 1
            continue
        dest = cache / "verify" / f"{ident}-{item.get('name', 'asset')}"
        try:
            download_with_fallback([item.get("url", "")], dest)
        except Exception as exc:
            print(f"FAIL {ident}: could not download - {exc}")
            bad += 1
            continue
        got, size = sha1_file(dest), dest.stat().st_size
        if got != h:
            print(f"FAIL {ident}: serves {got}, feed says {h}")
            bad += 1
        elif item.get("size") and int(item["size"]) != size:
            print(f"FAIL {ident}: serves {size} bytes, feed says {item['size']}")
            bad += 1
        else:
            print(f"ok   {ident}: {got} ({size:,} bytes)")
    print(f"\n{'all items verified' if not bad else f'{bad} item(s) wrong'}")
    return 0 if not bad else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate qol_update.json")
    ap.add_argument("-o", "--out", default="qol_update.json")
    ap.add_argument("--cache", default=".cll_update_cache")
    ap.add_argument(
        "--verify",
        metavar="FEED",
        help="Check an existing feed against the published assets, then exit.",
    )
    ap.add_argument(
        "--local",
        action="append",
        default=[],
        help="Use a local file instead of downloading. name=path or path",
    )
    args = ap.parse_args()

    cache = Path(args.cache)
    if args.verify:
        return verify_feed(Path(args.verify), cache)
    overrides = parse_overrides(args.local)
    items = []
    for spec in CATALOG:
        try:
            items.append(build_item(spec, cache, overrides))
        except Exception as exc:
            print(f"error {spec['id']}: {exc}", file=sys.stderr)
            return 1

    doc = {"source": HOSTED, "items": items}
    out = Path(args.out)
    out.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
