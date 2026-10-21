"""Validate the shipped Ferea warp in every supported move-list file."""

import pathlib
import struct
import unittest

from add_ferea import BUX, FILES, FEREA, RECORD


DATA_ROOT = pathlib.Path(__file__).resolve().parents[2] / "src/bin/Data/Local"


class FereaMoveListTest(unittest.TestCase):
    def test_ferea_matches_server_warp_in_every_locale(self):
        for relative in FILES:
            with self.subTest(file=relative):
                data = (DATA_ROOT / relative).read_bytes()
                count = struct.unpack_from("<i", data)[0]
                self.assertEqual(4 + 50 * RECORD.size, len(data))
                self.assertLessEqual(count, 50)
                entries = []
                for index in range(count):
                    encrypted = data[4 + index * RECORD.size:4 + (index + 1) * RECORD.size]
                    decoded = bytes(value ^ BUX[offset % 3] for offset, value in enumerate(encrypted))
                    entries.append(RECORD.unpack(decoded))
                matching = [entry for entry in entries if entry[0] == FEREA[0]]
                self.assertEqual(1, len(matching))
                number, name, alias, minimum, maximum, zen, gate = matching[0]
                self.assertEqual(b"Ferea", name.split(b"\0", 1)[0])
                self.assertEqual(b"Ferea", alias.split(b"\0", 1)[0])
                self.assertEqual((400, 400, 50000, 509), (minimum, maximum, zen, gate))


if __name__ == "__main__":
    unittest.main()
