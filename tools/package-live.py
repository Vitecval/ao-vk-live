#!/usr/bin/env python3
"""Package only AO-VK's own binaries and the reversible classic live installer."""
import hashlib
import importlib.util
import json
import pathlib
import shutil
import zipfile
import argparse

ROOT = pathlib.Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("preflight", ROOT / "tools/live-preflight.py")
preflight = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preflight)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--name", default="ao-vk-live-test4")
args = parser.parse_args()
if pathlib.Path(args.name).name != args.name:
    raise ValueError("Package name must be a single directory name")
package = ROOT / "dist" / args.name
package.mkdir(parents=True, exist_ok=False)
for name in ["ao-vk.dll", "ddraw.dll", "ao-vk.map"]:
    shutil.copy2(ROOT / "build/windows-live" / name, package / name)
shutil.copy2(ROOT / "tools/install-live.ps1", package / "install-live.ps1")
shutil.copy2(ROOT / "docs/live-client.md", package / "README.md")
shutil.copy2(ROOT / "LICENSE", package / "LICENSE.txt")
manifest = {
    "build": args.name,
    "client": preflight.EXPECTED,
    "addon": {name: hashlib.sha256((package / name).read_bytes()).hexdigest() for name in ["ao-vk.dll", "ddraw.dll"]},
    "previousAddons": [json.loads((ROOT / "dist" / previous / "manifest.json").read_text())["addon"]
                       for previous in ["ao-vk-live-test1", "ao-vk-live-test2", "ao-vk-live-test3"]
                       if (ROOT / "dist" / previous / "manifest.json").exists()],
}
(package / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
for name, option in [("Install", ""), ("Update", " -Upgrade"), ("Uninstall", " -Uninstall")]:
    (package / f"{name}.cmd").write_text(
        '@echo off\n'
        'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-live.ps1" '
        '-ClientDir "C:\\Funcom\\Anarchy Online"' + option + '\n'
        'set "result=%errorlevel%"\npause\nexit /b %result%\n'
    )
archive = package.with_suffix(".zip")
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
    for path in package.iterdir():
        z.write(path, package.name + "/" + path.name)
print(archive)
