"""Read AO records across physical data files without copying the database.

The index uses a logical offset. Each continuation file reserves block_offset
bytes for its header; a record (including its header) can straddle that boundary.
"""
import struct
from pathlib import Path


def read_record_at(database, offset, kind, identity, data_file_size, block_offset):
    if not 0 <= block_offset < data_file_size:
        raise ValueError('Invalid database file geometry')
    # The first file has no skipped prefix in the logical address space.
    if offset < data_file_size:
        part, position = 0, offset
    else:
        part, remainder = divmod(offset - data_file_size, data_file_size - block_offset)
        part, position = part + 1, remainder + block_offset

    def take(size):
        nonlocal part, position
        chunks = []
        while size:
            name = 'ResourceDatabase.dat' + (f'.{part:03}' if part else '')
            count = min(size, data_file_size - position)
            with (Path(database) / name).open('rb') as stream:
                stream.seek(position)
                chunk = stream.read(count)
            if len(chunk) != count:
                raise EOFError(f'Truncated database part {part}, offset {position}: expected {count}, got {len(chunk)}')
            chunks.append(chunk)
            size -= count
            position += count
            if position == data_file_size:
                part, position = part + 1, block_offset
        return b''.join(chunks)

    header = take(22)
    actual_kind, actual_id, size = struct.unpack_from('<III', header, 10)
    if (actual_kind, actual_id) != (kind, identity):
        raise ValueError(f'Record identity mismatch: {actual_kind}:{actual_id}, expected {kind}:{identity}')
    if not 0 < size < 100_000_000:
        raise ValueError(f'Invalid record length: {size}')
    return take(size)
