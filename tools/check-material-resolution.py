#!/usr/bin/env python3
"""Reject diffuse exports that lose native resolution or distort atlas aspect.

The size ledger contains dimensions only, read from full-quality game records.
Run against a staged pack tree before packaging. Missing source identities fail
closed; PNG image data and game database contents are never modified.
"""
import argparse
import json
import re
import struct
from pathlib import Path

DEFAULT_LEDGER = Path(__file__).resolve().parents[1] / 'material-packs/native-texture-dimensions.json'


def canonical(kind):
    return {1010016: 1010004, 1010017: 1010004, 1010021: 1010006,
            1010022: 1010006, 1010019: 1010011, 1010020: 1010011}.get(kind, kind)


def png_size(path):
    with path.open('rb') as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:16] != b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR':
        raise ValueError('Invalid PNG header')
    size = struct.unpack('>II', header[16:24])
    if not all(size):
        raise ValueError('Empty PNG dimension')
    return size


def require_size(identity, exported, native):
    width, height = exported
    original_width, original_height = native
    if width < original_width or height < original_height:
        raise ValueError(f'{identity}: {exported} is smaller than native {native}; recover or render full-quality source')
    if width * original_height != height * original_width:
        raise ValueError(f'{identity}: {exported} distorts native atlas aspect {native}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('packs', type=Path)
    parser.add_argument('--ledger', type=Path, default=DEFAULT_LEDGER)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    native = json.loads(args.ledger.read_text())['sizes']
    checked, errors = [], []
    for path in sorted(args.packs.rglob('*_d.png')):
        match = re.fullmatch(r'(\d+)_(\d+)_d.png', path.name)
        if not match:
            continue
        kind, identifier = map(int, match.groups())
        identity = f'{canonical(kind)}:{identifier}'
        try:
            if identity not in native:
                raise ValueError('Full-quality source dimensions are not in the ledger')
            size = png_size(path)
            require_size(identity, size, native[identity])
            checked.append({'path': str(path), 'identity': identity, 'size': size})
        except (ValueError, OSError) as error:
            errors.append({'path': str(path), 'identity': identity, 'error': str(error)})
    if not checked and not errors:
        errors.append({'error': 'No diffuse exports found; empty input cannot pass'})
    result = {'result': 'failed' if errors else 'passed', 'passedFiles': len(checked),
              'failedFiles': len(errors), 'errors': errors}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + '\n')
    print(f"{result['result']}: {len(checked)} valid diffuse exports, {len(errors)} failures")
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
