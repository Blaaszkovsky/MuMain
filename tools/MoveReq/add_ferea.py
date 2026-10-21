"""Add Ferea to the fixed-capacity Season 6 move-list files."""

import argparse
import os
import pathlib
import struct
import tempfile


RECORD = struct.Struct("<i32s32siiii")
BUX = (0xFC, 0xCF, 0xAB)
FEREA = (50, b"Ferea", b"Ferea", 400, 400, 50000, 509)
FILES = (
    "Movereq.bmd",
    "Eng/MoveReq_eng.bmd",
    "Por/MoveReq_por.bmd",
    "Spn/movereq_spn.bmd",
)


def convert(record):
    return bytes(value ^ BUX[index % len(BUX)] for index, value in enumerate(record))


def pack_name(name):
    if len(name) >= 32:
        raise ValueError("Move-list name exceeds its 32-byte field")
    return name.ljust(32, b"\0")


def add_ferea(path):
    data = path.read_bytes()
    if len(data) < 4 or (len(data) - 4) % RECORD.size:
        raise ValueError(f"Unexpected move-list size: {path}")

    count = struct.unpack_from("<i", data)[0]
    capacity = (len(data) - 4) // RECORD.size
    if not 0 <= count <= capacity:
        raise ValueError(f"Invalid move-list count: {path}")

    entry = (FEREA[0], pack_name(FEREA[1]), pack_name(FEREA[2]), *FEREA[3:])
    for offset in range(count):
        record = convert(data[4 + offset * RECORD.size:4 + (offset + 1) * RECORD.size])
        existing = RECORD.unpack(record)
        if existing[0] == FEREA[0]:
            if existing != entry:
                raise ValueError(f"Conflicting Ferea index: {path}")
            return False

    if count == capacity:
        raise ValueError(f"No free move-list slot: {path}")

    start = 4 + count * RECORD.size
    if convert(data[start:start + RECORD.size]) != bytes(RECORD.size):
        raise ValueError(f"Next move-list slot is not empty: {path}")

    updated = bytearray(data)
    struct.pack_into("<i", updated, 0, count + 1)
    updated[start:start + RECORD.size] = convert(RECORD.pack(*entry))
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
        temporary.write(updated)
        temporary_name = temporary.name
    os.replace(temporary_name, path)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[2] / "src/bin/Data/Local",
        help="Data/Local directory containing the move-list files",
    )
    arguments = parser.parse_args()
    for relative in FILES:
        path = arguments.root / relative
        print(f"{path}: {'updated' if add_ferea(path) else 'already present'}")


if __name__ == "__main__":
    main()
