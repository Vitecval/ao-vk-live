#!/usr/bin/env python3
"""Export a clean repository snapshot and a ready-built Windows player download."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIRS = ['cmake', 'docs', 'ghidra-scripts', 'interface', 'proxy', 'rvk', 'tests', 'third_party', 'tools', 'release', 'licenses', 'material-packs']
ROOT_FILES = ['.gitignore', 'CHANGELOG.md', 'CMakeLists.txt', 'CMakePresets.json', 'LICENSE',
              'THIRD-PARTY-NOTICES.md', 'RELEASE-NOTES.md']
EXCLUDE_PARTS = {'__pycache__', '.git', '.codex', '.agents', 'private'}
EXCLUDE_SUFFIXES = {'.dll', '.exe', '.lib', '.obj', '.pdb', '.pyc', '.log', '.cache', '.bmp'}
EXCLUDE_FILES = {'tools/build-windows.ps1', 'tools/package-live.py'}


def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(8<<20),b''):h.update(chunk)
    return h.hexdigest()


def archive(folder):
    result = Path(str(folder) + '.zip')
    with zipfile.ZipFile(result, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(folder.rglob('*')):
            if path.is_file():
                z.write(path, str(path.relative_to(folder.parent)))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='0.1.0')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/windows-release')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?', args.version):
        parser.error('Use a version such as 0.1.0 or 0.1.0-preview.1')
    build = args.build_dir.resolve()
    for name in ['ao-vk.dll', 'ddraw.dll']:
        if not (build / name).is_file():
            parser.error(f'Missing built file: {build / name}')
    out = args.output_dir or ROOT / 'dist/releases' / args.version
    out.mkdir(parents=True, exist_ok=False)
    prefix = f'ao-vk-live-{args.version}'
    player = out / (prefix + '-windows-x86-full')
    lite = out / (prefix + '-windows-x86-lite')
    source = out / (prefix + '-source')
    player.mkdir()
    source.mkdir()
    for name in ['ao-vk.dll', 'ddraw.dll']:
        shutil.copy2(build / name, player / name)
    for name in ['install-live.ps1', 'setup-live.ps1']:
        shutil.copy2(ROOT / 'tools' / name, player / name)
    for name in ['LICENSE', 'THIRD-PARTY-NOTICES.md', 'RELEASE-NOTES.md']:
        shutil.copy2(ROOT / name, player / name)
    shutil.copytree(ROOT / 'licenses', player / 'licenses')
    (player / 'docs').mkdir()
    for name in ['live-client.md', 'build-windows.md', 'publishing.md', 'weather.md', 'modern-materials.md', 'texture-rework.md', 'texture-overhaul-plan.md', 'preview12-validation.md']:
        shutil.copy2(ROOT / 'docs' / name, player / 'docs' / name)
    shutil.copy2(ROOT / 'material-packs/scan-coverage.json', player / 'docs/scan-coverage.json')
    shutil.copy2(ROOT / 'material-packs/building-families.json', player / 'docs/building-families.json')
    readme = (ROOT / 'docs/repository-readme.md').read_text(encoding='utf-8').replace('0.1.0', args.version)
    (player / 'README.md').write_text(readme, encoding='utf-8')
    for action in ['Install', 'Update', 'Uninstall']:
        (player / f'{action}.cmd').write_bytes((
            '@echo off\r\n'
            f'powershell.exe -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0setup-live.ps1" -Action {action}\r\n'
            'set "result=%errorlevel%"\r\n'
            'if not "%result%"=="0" pause\r\n'
            'exit /b %result%\r\n'
        ).encode('ascii'))
    spec = importlib.util.spec_from_file_location('live_preflight', ROOT / 'tools/live-preflight.py')
    preflight = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(preflight)
    known = json.loads((ROOT / 'release/known-builds.json').read_text(encoding='utf-8-sig'))
    manifest = {'product': 'AO-VK Live', 'version': args.version, 'build': prefix,
                'client': preflight.EXPECTED,
                'addon': {name: digest(player / name) for name in ['ao-vk.dll', 'ddraw.dll']},
                'previousAddons': [item['addon'] for item in known]}
    shutil.copytree(player,lite)
    editions=[]
    for edition,destination in [('full',player),('lite',lite)]:
        edition_manifest=dict(manifest,edition=edition,assets={},optionalTexturePack=edition=='full')
        for pack_name in ['ao-modern-icc','ao-cyber-icc','ao-cyber-characters','ao-cyber-world']:
            pack=ROOT/'material-packs'/pack_name
            pack_manifest=json.loads((pack/'manifest.json').read_text())
            materials=pack_manifest['materials']
            if edition=='lite':
                if pack_name not in ['ao-cyber-icc','ao-cyber-world']:continue
                materials=[x for x in materials if x['type']==1010006 and x['id'] in [17,20,21,22,23,24,25,26]]
            stems={f"{x['type']}_{x['id']}_" for x in materials}
            names={n for n in pack_manifest['files'] if any(n.startswith(stem) for stem in stems)}
            if not names:continue
            pack_dest=destination/'randy-vk/packs'/pack_name
            pack_dest.mkdir(parents=True)
            for name in sorted(names):
                if name not in pack_manifest['files'] or digest(pack/'maps'/name)!=pack_manifest['files'][name]:
                    raise RuntimeError(f'Unverified material map: {pack_name}/{name}')
                shutil.copy2(pack/'maps'/name,pack_dest/name)
            selected=dict(pack_manifest,materials=materials,files={n:pack_manifest['files'][n] for n in sorted(names)})
            if edition=='lite':selected['new_art_surfaces']=sum('d' in x['maps'] for x in materials)
            (pack_dest/'manifest.json').write_text(json.dumps(selected,indent=2)+'\n')
            shutil.copy2(pack/'README.md',pack_dest/'README.md')
            if edition=='full':
                for item in sorted(pack.glob('*catalog.json')):
                    shutil.copy2(item,pack_dest/item.name)
                # Authoring atlases are provenance, never an extra geometry pass.
                for atlas in sorted((pack/'source-art').glob('*atlas*.png')):
                    target=pack_dest/'source-art'/atlas.name
                    target.parent.mkdir(exist_ok=True)
                    shutil.copy2(atlas,target)
            edition_manifest['assets'].update({p.relative_to(destination).as_posix():digest(p) for p in sorted(pack_dest.rglob('*')) if p.is_file()})
        (destination/'manifest.json').write_text(json.dumps(edition_manifest,indent=2)+'\n',encoding='utf-8')
        editions.append(destination)
    derived=json.loads((ROOT/'release/derived-art.json').read_text())
    for directory in SOURCE_DIRS:
        for path in sorted((ROOT / directory).rglob('*')):
            rel = path.relative_to(ROOT)
            if not path.is_file() or EXCLUDE_PARTS.intersection(rel.parts):
                continue
            if path.suffix.lower() in EXCLUDE_SUFFIXES or rel.as_posix() in EXCLUDE_FILES:
                continue
            if rel.as_posix() in derived:
                if digest(path)!=derived[rel.as_posix()]['sha256']:raise RuntimeError(f'Derived source changed: {rel}')
                continue
            if directory=='material-packs' and len(rel.parts)>2 and rel.parts[2]=='maps':
                continue # deterministic baked outputs; original masters and all provenance are retained
            target = source / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    for name in ROOT_FILES:
        shutil.copy2(ROOT / name, source / name)
    (source / 'README.md').write_text(readme, encoding='utf-8')
    shutil.copy2(ROOT / 'README.md', source / 'docs/upstream-readme.md')
    (source/'SOURCE-ASSETS.md').write_text('''# Source artwork

All master artwork, atlases, catalogs and provenance are retained. Deterministic
baked map outputs and hash-verified duplicate atlas crops are omitted to keep
this archive within GitHub release limits. No workspace originals were removed.

Install Pillow and NumPy, then run:

    python tools/restore-source-art.py
    python tools/bake-modern-icc.py
    python tools/bake-cyber-icc.py
    python tools/bake-character-art.py
    python tools/bake-world-art.py

The crop restoration checks every result against release/derived-art.json and
refuses to replace differing files. The Full HD download includes pre-baked maps.
''',encoding='utf-8')
    # Reject unexpected proprietary/local artifacts in the exported source before archiving it.
    for path in source.rglob('*'):
        if path.is_file() and (path.suffix.lower() in EXCLUDE_SUFFIXES or 'randy-vk.ini' == path.name):
            raise RuntimeError(f'Unexpected local artifact in source: {path}')
    archives = [archive(folder) for folder in editions] + [archive(source)]
    for path in archives:
        if path.stat().st_size>=2**31:raise RuntimeError(f'GitHub release asset exceeds 2 GiB: {path}')
    (out / 'SHA256SUMS.txt').write_text(''.join(f'{digest(path)}  {path.name}\n' for path in archives), encoding='ascii')
    for path in archives:
        print(f'{path} ({path.stat().st_size / 1048576:.2f} MiB)')
    print(out / 'SHA256SUMS.txt')


if __name__ == '__main__':
    main()
