"""Add the Season 6 Crywolf warp without rewriting existing move-list records."""

import argparse
import os
import pathlib
import struct
import tempfile

from add_ferea import FILES, RECORD, convert, pack_name


CAPACITY = 50
HEADER_SIZE = 4
CRYWOLF = (26, b"Crywolf", b"Crywolf", 190, 400, 10000, 118)


def patch_data(data):
    # MoveCommandData::Create reads count + packed records, with no checksum.
    if len(data) != HEADER_SIZE + CAPACITY * RECORD.size:
        raise ValueError("Unexpected move-list size (no checksum/trailer is supported)")
    count = struct.unpack_from("<i", data)[0]
    if not 0 <= count <= CAPACITY:
        raise ValueError("Invalid move-list count")
    entry = (CRYWOLF[0], pack_name(CRYWOLF[1]), pack_name(CRYWOLF[2]), *CRYWOLF[3:])
    entries = [RECORD.unpack(convert(data[HEADER_SIZE + i * RECORD.size:HEADER_SIZE + (i + 1) * RECORD.size]))
               for i in range(count)]
    matching = [existing for existing in entries if existing[0] == CRYWOLF[0]]
    if matching:
        if len(matching) != 1 or matching[0] != entry:
            raise ValueError("Conflicting or duplicate Crywolf index")
        return data
    if count == CAPACITY:
        raise ValueError("No free move-list slot")
    start = HEADER_SIZE + count * RECORD.size
    if convert(data[start:start + RECORD.size]) != bytes(RECORD.size):
        raise ValueError("Next move-list slot is not empty")
    updated = bytearray(data)
    struct.pack_into("<i", updated, 0, count + 1)
    updated[start:start + RECORD.size] = convert(RECORD.pack(*entry))
    return bytes(updated)


def add_crywolf(path):
    original = path.read_bytes()
    updated = patch_data(original)
    if updated == original:
        return False
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
        temporary.write(updated)
        temporary_name = temporary.name
    os.replace(temporary_name, path)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[2] / "src/bin/Data/Local",
                        help="Data/Local directory containing the four move-list files")
    root = parser.parse_args().root
    for relative in FILES:
        path = root / relative
        print(f"{path}: {'updated' if add_crywolf(path) else 'already present'}")


if __name__ == "__main__":
    main()
