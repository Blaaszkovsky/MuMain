from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from terrain_bitmap import (BMP_FILE_HEADER, BMP_INFO_HEADER, BMP_HEADER_BYTES, CELL_COUNT,
                            SIDE, PIXEL_BYTES, bgr_to_rgb, convert_height, decode_legacy_height,
                            extended_heights, height_analysis, parse_terrain_ozb)


def fixture(pixels):
    header = BMP_FILE_HEADER.pack(b'BM', BMP_HEADER_BYTES + PIXEL_BYTES, 0, 0, BMP_HEADER_BYTES)
    header += BMP_INFO_HEADER.pack(40, SIDE, SIDE, 1, 24, 0, PIXEL_BYTES, 1000, 1000, 0, 0)
    return header[:4] + header + pixels


def packed_height(value):
    return (value + 500).to_bytes(3, 'big')


class TerrainBitmapTests(unittest.TestCase):
    def test_pixels_and_channel_order_roundtrip(self):
        pixels = bytes((4, 15, 230)) * CELL_COUNT
        self.assertEqual(pixels, parse_terrain_ozb(fixture(pixels)))
        rgb = bgr_to_rgb(pixels)
        self.assertEqual(bytes((230, 15, 4)), rgb[:3])
        self.assertEqual(pixels, bgr_to_rgb(rgb))

    def test_exact_legacy_height_grid_roundtrip(self):
        pixels = b''.join(packed_height((index % 128) * 3) for index in range(CELL_COUNT))
        self.assertTrue(height_analysis(pixels)['exact_legacy_possible'])
        output = convert_height(pixels)
        self.assertEqual(extended_heights(pixels), decode_legacy_height(output))
        self.assertEqual(66620, len(output))
        self.assertEqual(1080, struct.unpack_from('<I', output, 14)[0])

    def test_height_refuses_negative_range_and_quantization_loss(self):
        pixels = packed_height(0) * (CELL_COUNT - 3)
        pixels += packed_height(-1) + packed_height(383) + packed_height(1)
        analysis = height_analysis(pixels)
        self.assertEqual({'negative': 1, 'above_legacy_range': 1, 'off_legacy_grid': 1}, analysis['failure_counts'])
        self.assertEqual(3, analysis['nonrepresentable_samples'])
        with self.assertRaises(ValueError):
            convert_height(pixels)

    def test_rejects_truncation_trailing_bytes_and_bad_prefix(self):
        data = fixture(bytes(PIXEL_BYTES))
        for malformed in (data[:-1], data + b'\0', b'xxxx' + data[4:]):
            with self.assertRaises(ValueError):
                parse_terrain_ozb(malformed)

    def test_rejects_unsupported_headers(self):
        data = fixture(bytes(PIXEL_BYTES))
        # Absolute offsets: BMP pixel offset, DIB size, width, height, bits, compression.
        changes = ((14, '<I', 1080), (18, '<I', 108), (22, '<i', 512),
                   (26, '<i', -256), (32, '<H', 32), (34, '<I', 1))
        for offset, format_string, value in changes:
            malformed = bytearray(data)
            struct.pack_into(format_string, malformed, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                parse_terrain_ozb(malformed)

    def test_rejects_wrong_pixel_and_legacy_buffer_sizes(self):
        for operation in (bgr_to_rgb, extended_heights, decode_legacy_height):
            with self.assertRaises(ValueError):
                operation(bytes(10))


if __name__ == '__main__':
    unittest.main()
