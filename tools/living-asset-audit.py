"""Inspect installed-client compatibility without loading DLLs or extracting assets.

Export presence is evidence of an interface, not a tested ABI or a permission grant.
This tool deliberately never opens ResourceDatabase.dat or an old Living cache.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parent.parent
INTERFACES = {
    'N3.dll': [
        '?Get@n3DatabaseHandler_t@@SAAAV1@XZ',
        '?GetResourceDatabase@n3DatabaseHandler_t@@QBEAAVResourceDatabase_t@@XZ',
    ],
    'DatabaseController.dll': [
        '?GetDbObject@ResourceDatabase_t@@UAEPAVDbObject_t@@ABVIdentity_t@@@Z',
    ],
    'ResourceManager.dll': ['?Get@ResourceManager@@SAAAV1@XZ'],
}


def exports(data):
    """Bounded PE32 name-table inspection; no LoadLibrary or native calls."""
    def unpack(fmt, at):
        if at < 0 or at + struct.calcsize(fmt) > len(data):
            raise ValueError('Truncated PE structure')
        return struct.unpack_from(fmt, data, at)
    if data[:2] != b'MZ':
        raise ValueError('Not a PE image')
    pe, = unpack('<I', 60)
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Missing PE signature')
    machine, count = unpack('<HH', pe+4)
    optional_size, = unpack('<H', pe+20)
    if machine != 0x14c or not 1 <= count <= 96 or optional_size < 104:
        raise ValueError('Not a supported PE32 image')
    optional = pe + 24
    if unpack('<H', optional)[0] != 0x10b or unpack('<I', optional+92)[0] < 1:
        raise ValueError('Missing PE32 export directory')
    sections = []
    for index in range(count):
        size, rva, raw_size, raw = unpack('<IIII', optional+optional_size+index*40+8)
        sections.append((rva, raw_size, raw))
    def offset(rva, size):
        for start, raw_size, raw in sections:
            if start <= rva and rva-start+size <= raw_size:
                at = raw+rva-start
                if at+size <= len(data):
                    return at
        raise ValueError('PE RVA outside file-backed sections')
    rva, size = unpack('<II', optional+96)
    if not rva or size < 40:
        return set()
    table = offset(rva, 40)
    count, = unpack('<I', table+24)
    names, = unpack('<I', table+32)
    if count > 65536:
        raise ValueError('PE export count exceeds budget')
    names_at = offset(names, count*4) if count else 0
    result = set()
    for index in range(count):
        at = offset(unpack('<I', names_at+index*4)[0], 1)
        end = data.find(b'\0', at, min(at+4096, len(data)))
        if end < 0:
            raise ValueError('Unterminated export name')
        result.add(data[at:end].decode('ascii'))
    return result


def inspect(client):
    client = Path(client).resolve(strict=True)
    spec = importlib.util.spec_from_file_location('live_preflight', ROOT/'tools/live-preflight.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    files = {}
    for name in sorted(set(module.EXPECTED) | set(INTERFACES)):
        path = client/name
        item = {'present': path.is_file()}
        if item['present']:
            # Client DLLs are small. Refuse arbitrary oversized input before reading.
            if path.stat().st_size > 64 << 20:
                item['error'] = 'DLL exceeds audit budget'
            else:
                data = path.read_bytes()
                item['sha256'] = hashlib.sha256(data).hexdigest()
                if name in module.EXPECTED:
                    item['supported_client_hash'] = item['sha256'] == module.EXPECTED[name]
                if name in INTERFACES:
                    try:
                        names = exports(data)
                        item['interfaces'] = {key: key in names for key in INTERFACES[name]}
                    except (ValueError, UnicodeError) as error:
                        item['error'] = str(error)
        files[name] = item
    return {
        'schema': 1, 'client': str(client), 'read_only': True,
        'supported_renderer_client': all(files[name].get('supported_client_hash', False) for name in module.EXPECTED),
        'database_exports_present': all(all(files[name].get('interfaces', {}).get(key, False) for key in keys) for name, keys in INTERFACES.items()),
        'provider': 'none', 'status': 'unsupported',
        'reason': 'Database object access exists in local AOSharp bindings; mesh/material/animation ownership and renderer handoff are not validated.',
        'permissions': 'not established by local ownership or export presence',
        'resource_bytes_read': 0, 'resources_extracted': 0,
        'files': files,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.client), indent=2))


if __name__ == '__main__':
    main()
