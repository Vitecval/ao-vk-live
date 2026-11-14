"""Refresh Preview 24 bootstraps while preserving its pinned material companions."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.6.0-preview.24'
SCRIPTS = ['setup-safety.ps1', 'install-live.ps1', 'setup-live.ps1',
           'reset-settings.ps1', 'setup-progress.ps1', 'material-download.ps1',
           'material-parts.ps1', 'material-cache.ps1', 'large-address-aware.ps1',
           'mesh-index.ps1', 'startup-report.ps1']

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def write(path, data):
    Path(path).write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--source-manifest', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    args = parser.parse_args()
    assert len(args.commit) == 40 and all(c in '0123456789abcdef' for c in args.commit)
    out = args.output.resolve()
    assert not out.exists(), 'Choose a new output; preserve earlier packages.'
    source = read(args.source_manifest)
    for item in source['files']:
        assert sha(ROOT / item['path']) == item['sha256'], item['path']
    source_hash = sha(args.source_manifest)
    base = ROOT / 'dist/releases' / VERSION
    pinned = {x['name']: x['sha256'] for x in read(base / 'PACKAGE-AUDIT.json')['products']}
    r52 = ROOT / 'build/preview24/stability-r52'
    candidate = args.candidate.resolve()
    verified = read(candidate / 'VERIFIED.json')
    revision = int(verified['revision'])
    assert revision >= 53
    renderer = candidate / 'candidate/ao-vk.dll'
    assert verified['result'] == 'passed' and sha(renderer) == verified['rendererSha256']
    binaries = {'ao-vk.dll': renderer, **{n: r52 / 'candidate' / n for n in ['ddraw.dll', 'ao-vk-cache-warm.exe']}}
    known = [read(r52 / 'F-BASELINE-RECEIPT.json')['addon']]
    for folder in [r52, candidate]:
        known.append({'ao-vk.dll': sha(folder / 'candidate/ao-vk.dll'), 'ddraw.dll': sha(binaries['ddraw.dll'])})
        known.append({'ao-vk.dll': sha(folder / 'candidate/ao-vk.dll'), 'ddraw.dll': known[0]['ddraw.dll']})
    # Recognize the earlier locally tested renderer revisions on update too.
    for pattern in ['build/preview24/*/candidate/ao-vk.dll', 'build/preview23/atmosphere-*/candidate/ao-vk.dll']:
        for file in sorted(ROOT.glob(pattern)):
            for loader in {x['ddraw.dll'] for x in known}:
                record = {'ao-vk.dll': sha(file), 'ddraw.dll': loader}
                if record not in known:
                    known.append(record)
    out.mkdir(parents=True)
    products = []
    for edition, suffix in [('lite', 'lite'), ('full', 'full-setup')]:
        name = f'ao-vk-live-{VERSION}-windows-x86-{suffix}'
        seed = base / (name + '.zip')
        assert sha(seed) == pinned[seed.name]
        folder = out / name
        folder.mkdir()
        with zipfile.ZipFile(seed) as archive:
            assert archive.testzip() is None
            for item in archive.infolist():
                parts = item.filename.split('/')
                assert len(parts) > 1 and '..' not in parts and not item.filename.startswith('/')
                relative = Path(*parts[1:])
                if item.is_dir():
                    (folder / relative).mkdir(parents=True, exist_ok=True)
                else:
                    target = folder / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with archive.open(item) as src, target.open('wb') as dst:
                        shutil.copyfileobj(src, dst)
        manifest = read(folder / 'manifest.json')
        old_addon_bytes = sum((folder / n).stat().st_size for n in manifest['addon'])
        old_addon = dict(manifest['addon'])
        parts_before = manifest.get('materialParts')
        for name, file in binaries.items():
            shutil.copy2(file, folder / name)
        for name in SCRIPTS:
            shutil.copy2(ROOT / 'tools' / name, folder / name)
        for name in ['README.md', 'RELEASE-NOTES.md', 'PLAYTEST.md', 'docs/preview24.md']:
            shutil.copy2(ROOT / 'docs/preview24.md', folder / name)
        manifest.update(build='ao-vk-live-' + VERSION, setupRevision=revision-43,
                        candidateRevision=revision, candidateStatus='release',
                        publicDistributionApproved=True, sourceCommit=args.commit,
                        sourceManifestSha256=source_hash)
        manifest.pop('setupSourceCommit', None)
        manifest.pop('localRevision', None)
        manifest['addon'] = {name: sha(folder / name) for name in ['ao-vk.dll', 'ddraw.dll']}
        manifest['installBytes'] += sum((folder / n).stat().st_size for n in manifest['addon']) - old_addon_bytes
        for record in [old_addon, *known]:
            if record != manifest['addon'] and record not in manifest['previousAddons']:
                manifest['previousAddons'].insert(0, record)
        manifest['cacheWarmup']['sha256'] = sha(folder / 'ao-vk-cache-warm.exe')
        # Full's assets in companion archives keep their original pinned hashes.
        for file in folder.rglob('*'):
            if file.is_file() and file.name != 'manifest.json':
                relative = file.relative_to(folder).as_posix()
                assert relative in manifest['payload'], relative
                manifest['payload'][relative] = sha(file)
                if relative in manifest['assets']:
                    manifest['assets'][relative] = sha(file)
        if edition == 'lite':
            assert not any(manifest.get(k) for k in ['materialParts', 'materialDownload', 'preparedMaterials'])
            assert not any(n.startswith(('randy-vk/packs/', 'randy-vk/cache/', 'randy-vk/materials/')) for n in manifest['payload'])
        else:
            assert manifest['materialParts'] == parts_before and len(parts_before) == 19
            assert manifest['preparedMaterials']['files'] == 14345
        write(folder / 'manifest.json', manifest)
        target = out / (folder.name + '.zip')
        with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for file in sorted(folder.rglob('*')):
                if file.is_file():
                    archive.write(file, folder.name + '/' + file.relative_to(folder).as_posix())
        with zipfile.ZipFile(target) as archive:
            assert archive.testzip() is None
            for relative, digest in manifest['payload'].items():
                key = folder.name + '/' + relative
                if key in archive.namelist():
                    assert hashlib.sha256(archive.read(key)).hexdigest() == digest, relative
                else:
                    assert edition == 'full' and relative in manifest['assets'], relative
        products.append({'name': target.name, 'bytes': target.stat().st_size, 'sha256': sha(target)})
        print(edition, 'verified', target.stat().st_size, flush=True)
    for record in read(base / 'PACKAGE-AUDIT.json')['products']:
        if '-full-materials-' in record['name']:
            products.append(record)
    write(out / 'PACKAGE-AUDIT.json', {'result': 'passed', 'revision': revision,
          'sourceCommit': args.commit, 'sourceManifestSha256': source_hash,
          'liteTextureDownloads': False, 'fullPreparedFiles': 14345,
          'unchangedMaterialParts': 19, 'products': products})
    (out / 'SHA256SUMS.txt').write_text(''.join(f"{p['sha256']}  {p['name']}\n" for p in products), encoding='ascii')

if __name__ == '__main__':
    main()
