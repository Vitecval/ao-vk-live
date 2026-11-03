#!/usr/bin/env python3
"""Export only model identifiers and irreversible hashes, never game meshes/names."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def sanitize(data):
    if len(data) < 20 or data[:8] != b'AOMESHID':
        raise ValueError('Not an AOMESHID index')
    version, count, names = struct.unpack_from('<III', data, 8)
    if version != 1 or not 0 < count <= 1000000 or not names or len(data) != 20 + count * 32 + names:
        raise ValueError('Invalid mesh-index header/length')
    rows = list(struct.iter_unpack('<QQIIHHI', data[20:20 + count * 32]))
    if any(a[0] > b[0] for a, b in zip(rows, rows[1:])):
        raise ValueError('Index is not sorted by geometry hash')
    if any(r[2] not in (1010001, 1010026) or r[6] >= names for r in rows):
        raise ValueError('Invalid model type/name offset')
    result = bytearray(b'AOMESHID' + struct.pack('<III', 1, count, 1))
    for row in rows:
        result.extend(struct.pack('<QQIIHHI', *row[:6], 0))
    result.append(0)
    return bytes(result), rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='Locally generated version-1 identification index')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = args.input.read_bytes()
    output, rows = sanitize(source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    report = dict(format='AOMESHID', formatVersion=1, records=len(rows),
                  staticModels=len({(r[2], r[3]) for r in rows}),
                  sourceSha256=hashlib.sha256(source).hexdigest(),
                  sha256=hashlib.sha256(output).hexdigest(), bytes=len(output),
                  containsGeometry=False, containsArtwork=False, containsNames=False,
                  identityRecordsUnchanged=True)
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
