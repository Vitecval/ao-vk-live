"""Published078 Full policy regression; extracts metadata, never artwork/binaries.

The transaction fixture uses synthetic bytes and an isolated fake AO folder.
No published ZIP, current client, launcher or renderer build is modified.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=root / "build/horizon-0.7.8/publish/ao-vk-live-0.6.0-preview.24-windows-x86-full-setup.zip")
    parser.add_argument("--baseline-core", type=Path, help="Use a preserved old core for the rejection control while testing the fixed candidate manifest and exact safety bytes")
    parser.add_argument("--output", type=Path, default=root / "build/horizon-0.7.8/asset-policy-fix-tests")
    parser.add_argument("--powershell", default=shutil.which("pwsh"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    receipt = {"result": "failed", "core": str(args.core.resolve()), "publishedCoreSha256": sha256(args.core), "patchedSafetySha256": sha256(root / "tools/setup-safety.ps1")}
    try:
        if not args.powershell:
            raise RuntimeError("PowerShell Core required for isolated script fixture")
        with tempfile.TemporaryDirectory(prefix="ao-public078-policy-") as scratch:
            scratch = Path(scratch)
            with zipfile.ZipFile(args.core) as archive:
                manifests = [n for n in archive.namelist() if Path(n).name == "manifest.json" and n.count("/") <= 1]
                safety = [n for n in archive.namelist() if Path(n).name == "setup-safety.ps1"]
                if len(manifests) != 1 or len(safety) != 1:
                    raise RuntimeError("Expected one top-level Full manifest and safety script")
                manifest_bytes = archive.read(manifests[0])
                old_safety = archive.read(safety[0])
            if args.baseline_core:
                receipt["candidateSafetySha256"] = hashlib.sha256(old_safety).hexdigest()
                if receipt["candidateSafetySha256"] != receipt["patchedSafetySha256"]:
                    raise RuntimeError("Candidate does not contain the exact safety implementation exercised by this fixture")
                receipt["baselineCore"] = str(args.baseline_core.resolve())
                receipt["baselineCoreSha256"] = sha256(args.baseline_core)
                with zipfile.ZipFile(args.baseline_core) as baseline:
                    names = [n for n in baseline.namelist() if Path(n).name == "setup-safety.ps1"]
                    if len(names) != 1:
                        raise RuntimeError("Expected one preserved baseline safety script")
                    old_safety = baseline.read(names[0])
            receipt["publishedManifestSha256"] = hashlib.sha256(manifest_bytes).hexdigest()
            receipt["publishedSafetySha256"] = hashlib.sha256(old_safety).hexdigest()
            (scratch / "published-manifest.json").write_bytes(manifest_bytes)
            (scratch / "published-safety.ps1").write_bytes(old_safety)
            process = subprocess.run([
                args.powershell, "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(root / "tests/public_full_asset_policy.tests.ps1"),
                "-PublishedManifest", str(scratch / "published-manifest.json"), "-PublishedSafety", str(scratch / "published-safety.ps1"),
                "-FixtureRoot", str(scratch / "fixture"), "-ResultsPath", str(scratch / "results.json"),
            ], capture_output=True, text=True, timeout=180)
            (args.output / "fixture.log").write_text(process.stdout + process.stderr, encoding="utf-8")
            if (scratch / "results.json").exists():
                receipt["policyAndTransaction"] = json.loads((scratch / "results.json").read_text(encoding="utf-8-sig"))
            if process.returncode:
                raise RuntimeError(f"Isolated PowerShell test failed ({process.returncode}); see fixture.log")
            if receipt.get("policyAndTransaction", {}).get("result") != "passed":
                raise RuntimeError("Missing successful fixture result")
        if sha256(args.core) != receipt["publishedCoreSha256"]:
            raise RuntimeError("Published core changed during test")
        if args.baseline_core and sha256(args.baseline_core) != receipt["baselineCoreSha256"]:
            raise RuntimeError("Preserved baseline core changed during test")
        receipt["publishedCoreUnchanged"] = True
        receipt["result"] = "passed"
    except Exception as exc:
        receipt["error"] = str(exc)
    (args.output / "RESULTS.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, indent=2))
    return 0 if receipt["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
