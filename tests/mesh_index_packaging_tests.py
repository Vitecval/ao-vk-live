"""Run the production packager with small synthetic artwork; assert dependency closure."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--output', type=Path, required=True)
    out = p.parse_args().output.resolve(); out.mkdir(parents=True, exist_ok=True)
    fixture = out / ('run-' + uuid.uuid4().hex[:8]); fixture.mkdir()
    for folder in ['tools', 'release', 'material-packs', 'build-fixture']:
        (fixture / folder).mkdir()
    for n in ['package-release.py', 'live-preflight.py', 'install-live.ps1', 'setup-live.ps1',
              'material-parts.ps1', 'material-download.ps1', 'large-address-aware.ps1', 'mesh-index.ps1']:
        shutil.copy2(ROOT / 'tools' / n, fixture / 'tools' / n)
    for folder in ['docs', 'licenses']:
        shutil.copytree(ROOT / folder, fixture / folder)
    shutil.copytree(ROOT / 'release/model-index', fixture / 'release/model-index')
    for n in ['large-address-aware.json', 'known-builds.json']:
        shutil.copy2(ROOT / 'release' / n, fixture / 'release' / n)
    for n in ['LICENSE', 'LICENSE-TEXTURES.md', 'THIRD-PARTY-NOTICES.md', 'RELEASE-NOTES.md']:
        shutil.copy2(ROOT / n, fixture / n)
    for n in ['ao-vk.dll', 'ddraw.dll', 'ao-vk-cache-warm.exe']:
        (fixture / 'build-fixture' / n).write_bytes(('synthetic ' + n).encode())
    for n in ['scan-coverage.json', 'building-families.json', 'rubika-worklist.json', 'art-publication.json']:
        (fixture / 'material-packs' / n).write_text('{}')
    for pack in ['ao-modern-icc', 'ao-cyber-icc', 'ao-cyber-characters', 'ao-cyber-world']:
        dest = fixture / 'material-packs' / pack; (dest / 'maps').mkdir(parents=True)
        name = '1010006_17_d.png'; path = dest / 'maps' / name; path.write_bytes(b'synthetic map')
        m = dict(materials=[dict(type=1010006, id=17, maps={'d':name})], files={name:sha(path)})
        (dest / 'manifest.json').write_text(json.dumps(m)); (dest / 'README.md').write_text('Synthetic fixture')
    (fixture / 'material-packs/ao-cyber-icc/deck-family.json').write_text('{"members":[{"id":17}]}')
    output = fixture / 'output'
    command = [sys.executable, str(fixture / 'tools/package-release.py'), '--no-source', '--defer-archives',
               '--version', '0.6.0-preview.21', '--build-dir', str(fixture / 'build-fixture'), '--output-dir', str(output)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=180)
    (out / 'packager.log').write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stderr
    cases = []
    for edition in ['lite', 'full']:
        package = output / f'ao-vk-live-0.6.0-preview.21-windows-x86-{edition}'
        m = json.loads((package / 'manifest.json').read_text()); relative = 'randy-vk/mesh-ids.bin'
        assert m['setupRevision'] == 4 and m['meshIndex']['file'] == relative
        assert sha(package / relative) == m['meshIndex']['sha256'] == m['assets'][relative] == m['payload'][relative]
        assert (package / 'mesh-index.ps1').read_bytes() == (ROOT / 'tools/mesh-index.ps1').read_bytes()
        assert (package / 'docs/antivirus.md').exists()
        for n,h in m['payload'].items(): assert sha(package / n) == h, n
        cases.append(dict(edition=edition, result='passed', required_dependency_in_payload_and_assets=True))
    # A future release build must fail if the required dependency is absent.
    (fixture / 'release/model-index/mesh-ids.bin').unlink()
    bad = fixture / 'missing-index-output'; command[-1] = str(bad)
    result = subprocess.run(command, capture_output=True, text=True, timeout=180)
    assert result.returncode != 0 and not list(bad.glob('*.zip'))
    (out / 'RESULTS.json').write_text(json.dumps(dict(result='passed', cases=cases,
        missing_required_index_refuses_packaging=True), indent=2) + '\n')
    print('PASS: production Lite/Full packager includes and pins the required index; missing dependency refuses packaging.')


if __name__ == '__main__': main()
