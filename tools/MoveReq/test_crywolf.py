"""Check the Crywolf move-list contract used by the Season 6 server."""

import pathlib
import struct
import unittest

from add_ferea import FILES, RECORD, convert
from add_crywolf import CAPACITY, CRYWOLF, HEADER_SIZE, patch_data


DATA_ROOT = pathlib.Path(__file__).resolve().parents[2] / "src/bin/Data/Local"
CRYWOLF_INDEX = 26
CRYWOLF_REQUIREMENTS = (190, 400, 10000, 118)


class CrywolfMoveListTest(unittest.TestCase):
    def fixture_data(self, entries):
        return struct.pack("<i", len(entries)) + b"".join(convert(RECORD.pack(*entry)) for entry in entries) + \
            convert(bytes(RECORD.size)) * (CAPACITY - len(entries))

    def crywolf_entry(self):
        return (CRYWOLF[0], CRYWOLF[1].ljust(32, b"\0"), CRYWOLF[2].ljust(32, b"\0"), *CRYWOLF[3:])

    def test_patch_preserves_old_record_roundtrip_and_unused_tail(self):
        old = (3, b"Existing".ljust(32, b"\0"), b"Alias".ljust(32, b"\0"), 10, 400, 100, 17)
        original = self.fixture_data([old])
        updated = patch_data(original)
        self.assertEqual(2, struct.unpack_from("<i", updated)[0])
        self.assertEqual(original[HEADER_SIZE:HEADER_SIZE + RECORD.size],
                         updated[HEADER_SIZE:HEADER_SIZE + RECORD.size])
        self.assertEqual(original[HEADER_SIZE + 2 * RECORD.size:], updated[HEADER_SIZE + 2 * RECORD.size:])
        self.assertEqual(original, convert(convert(original)))
        added = updated[HEADER_SIZE + RECORD.size:HEADER_SIZE + 2 * RECORD.size]
        self.assertEqual(self.crywolf_entry(), RECORD.unpack(convert(added)))

    def test_patch_is_idempotent(self):
        updated = patch_data(self.fixture_data([]))
        self.assertEqual(updated, patch_data(updated))

    def test_rejects_duplicate_or_conflicting_id(self):
        entry = self.crywolf_entry()
        for entries in ([entry, entry], [(entry[0], *entry[1:3], 1, *entry[4:])]):
            with self.subTest(entries=len(entries)), self.assertRaises(ValueError):
                patch_data(self.fixture_data(entries))

    def test_rejects_occupied_next_slot(self):
        original = bytearray(self.fixture_data([]))
        original[HEADER_SIZE] ^= 1
        with self.assertRaises(ValueError):
            patch_data(original)

    def test_rejects_bad_count_or_checksum_trailer(self):
        original = self.fixture_data([])
        invalid_count = struct.pack("<i", CAPACITY + 1) + original[HEADER_SIZE:]
        for invalid in (invalid_count, original[:-1], original + b"\0\0\0\0"):
            with self.subTest(size=len(invalid)), self.assertRaises(ValueError):
                patch_data(invalid)

    def test_shipped_crywolf_matches_server_warp_in_every_locale(self):
        for relative in FILES:
            with self.subTest(file=relative):
                data = (DATA_ROOT / relative).read_bytes()
                count = struct.unpack_from("<i", data)[0]
                entries = [RECORD.unpack(convert(data[4 + index * RECORD.size:4 + (index + 1) * RECORD.size]))
                           for index in range(count)]
                matches = [entry for entry in entries if entry[0] == CRYWOLF_INDEX]
                self.assertEqual(1, len(matches))
                entry = matches[0]
                self.assertEqual(b"Crywolf", entry[1].split(b"\0", 1)[0])
                self.assertEqual(b"Crywolf", entry[2].split(b"\0", 1)[0])
                self.assertEqual(CRYWOLF_REQUIREMENTS, entry[3:])


if __name__ == "__main__":
    unittest.main()
