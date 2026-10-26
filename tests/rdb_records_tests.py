"""Exercise split headers/payloads, continuation offsets and corrupt records."""
import importlib.util
import struct
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('records', ROOT/'tools/rdb_records.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as folder:
    db = Path(folder)
    span, prefix = 128, 16
    for offset in [30, 109, 120, 128, 227, 350]:
        payload = bytes(range(256))*3
        record = bytes(10) + struct.pack('<III', 1010004, 17824, len(payload)) + payload
        logical = bytearray(offset) + record
        first, remaining = logical[:span], logical[span:]
        (db/'ResourceDatabase.dat').write_bytes(first.ljust(span, b'\0'))
        part = 1
        while remaining:
            chunk, remaining = remaining[:span-prefix], remaining[span-prefix:]
            (db/f'ResourceDatabase.dat.{part:03}').write_bytes(bytes(prefix)+chunk.ljust(span-prefix,b'\0'))
            part += 1
        assert m.read_record_at(db, offset, 1010004, 17824, span, prefix) == payload, offset
        try: m.read_record_at(db, offset, 1010004, 999, span, prefix)
        except ValueError: pass
        else: raise AssertionError('Wrong identity accepted')
    (db/'ResourceDatabase.dat.001').write_bytes(b'bad')
    try: m.read_record_at(db, 120, 1010004, 17824, span, prefix)
    except EOFError: pass
    else: raise AssertionError('Truncation accepted')
print('Split record headers, payloads, logical offsets and corruption: passed')
