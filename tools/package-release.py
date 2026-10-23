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
SOURCE_DIRS = ['cmake', 'docs', 'ghidra-scripts', 'interface', 'proxy', 'rvk', 'tests', 'third_party', 'tools', 'release', 'licenses']
ROOT_FILES = ['.gitignore', 'CHANGELOG.md', 'CMakeLists.txt', 'CMakePresets.json', 'LICENSE',
              'THIRD-PARTY-NOTICES.md', 'RELEASE-NOTES.md']
EXCLUDE_PARTS = {'__pycache__', '.git', '.codex', '.agents', 'private'}
EXCLUDE_SUFFIXES = {'.dll', '.exe', '.lib', '.obj', '.pdb', '.pyc', '.log', '.cache', '.bmp'}
EXCLUDE_FILES = {'tools/build-windows.ps1', 'tools/package-live.py'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
    player = out / (prefix + '-windows-x86')
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
    for name in ['live-client.md', 'build-windows.md', 'publishing.md']:
        shutil.copy2(ROOT / 'docs' / name, player / 'docs' / name)
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
    (player / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    for directory in SOURCE_DIRS:
        for path in sorted((ROOT / directory).rglob('*')):
            rel = path.relative_to(ROOT)
            if not path.is_file() or EXCLUDE_PARTS.intersection(rel.parts):
                continue
            if path.suffix.lower() in EXCLUDE_SUFFIXES or rel.as_posix() in EXCLUDE_FILES:
                continue
            target = source / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    for name in ROOT_FILES:
        shutil.copy2(ROOT / name, source / name)
    (source / 'README.md').write_text(readme, encoding='utf-8')
    shutil.copy2(ROOT / 'README.md', source / 'docs/upstream-readme.md')
    # Reject unexpected proprietary/local artifacts in the exported source before archiving it.
    for path in source.rglob('*'):
        if path.is_file() and (path.suffix.lower() in EXCLUDE_SUFFIXES or 'randy-vk.ini' == path.name):
            raise RuntimeError(f'Unexpected local artifact in source: {path}')
    archives = [archive(player), archive(source)]
    (out / 'SHA256SUMS.txt').write_text(''.join(f'{digest(path)}  {path.name}\n' for path in archives), encoding='ascii')
    for path in archives:
        print(f'{path} ({path.stat().st_size / 1048576:.2f} MiB)')
    print(out / 'SHA256SUMS.txt')


if __name__ == '__main__':
    main()
