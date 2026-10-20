"""Explicit native API checks; run --library <trusted TurboJPEG 3 shared library>."""
import argparse
import json
from pathlib import Path
import random
import sys
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lossless_jpeg import LosslessJpegCodec, validate_lossless_headers, OZJ_PREFIX_BYTES, JPEG_CAPACITY
from terrain_bitmap import SIDE, PIXEL_BYTES
from test_terrain_bitmap import fixture, packed_height

LIBRARY = None


class LosslessJpegTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.codec = LosslessJpegCodec(LIBRARY)

    def test_exact_channels_and_bottom_up_orientation(self):
        pixels = bytes(value for y in range(SIDE) for x in range(SIDE) for value in (y, x, (x + y) % 256))
        output = self.codec.encode(pixels)
        self.assertEqual(pixels, self.codec.decode_like_mumain(output))
        self.assertEqual(output[:OZJ_PREFIX_BYTES], output[OZJ_PREFIX_BYTES:2 * OZJ_PREFIX_BYTES])

    def test_high_entropy_stays_in_fixed_capacity(self):
        pixels = random.Random(20261020).randbytes(PIXEL_BYTES)
        output = self.codec.encode(pixels)
        self.assertLessEqual(len(output), JPEG_CAPACITY + OZJ_PREFIX_BYTES)
        self.assertEqual(pixels, self.codec.decode_like_mumain(output))

    def test_refuses_wrong_input_sizes_and_non_jpeg(self):
        for data in (b'', bytes(PIXEL_BYTES - 1), bytes(PIXEL_BYTES + 1)):
            with self.assertRaises(ValueError):
                self.codec.encode(data)
        for data in (b'', bytes(100), bytes(JPEG_CAPACITY + OZJ_PREFIX_BYTES + 1)):
            with self.assertRaises(ValueError):
                self.codec.decode_like_mumain(data)

    def test_refuses_wrong_dimensions_predictor_and_point_transform(self):
        jpeg = self.codec.encode(bytes(PIXEL_BYTES))[OZJ_PREFIX_BYTES:]
        frame = jpeg.index(b'\xff\xc3')
        scan = jpeg.index(b'\xff\xda')
        length = int.from_bytes(jpeg[scan + 2:scan + 4], 'big')
        for offset, value in ((frame + 7, 2), (scan + length - 1, 2), (scan + length + 1, 1)):
            damaged = bytearray(jpeg)
            damaged[offset] = value
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                validate_lossless_headers(damaged)

    def test_cli_blocks_ferea_like_height_and_preserves_inputs(self):
        script = Path(__file__).resolve().parents[1] / 'convert_terrain_images.py'
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / 'World113'
            source.mkdir()
            height = fixture(packed_height(-1) * (SIDE * SIDE))
            light = fixture(bytes((1, 2, 3)) * (SIDE * SIDE))
            (source / 'TerrainHeight.OZB').write_bytes(height)
            (source / 'TerrainLight.OZB').write_bytes(light)
            output = base / 'output'
            command = [sys.executable, str(script), '--world', str(source), '--output', str(output),
                       '--turbojpeg', str(LIBRARY)]
            result = subprocess.run(command, capture_output=True)
            self.assertEqual(1, result.returncode, result.stderr)
            manifest = json.loads((output / 'manifest.json').read_text())
            self.assertEqual({'blocked': 1, 'converted': 1}, manifest['summary'])
            self.assertFalse((output / 'TerrainHeight.OZB').exists())
            self.assertTrue((output / 'TerrainLight.OZJ').exists())
            self.assertEqual(height, (source / 'TerrainHeight.OZB').read_bytes())
            self.assertEqual(light, (source / 'TerrainLight.OZB').read_bytes())
            self.assertEqual(2, subprocess.run(command, capture_output=True).returncode)

    def test_cli_converts_representable_height_and_light(self):
        script = Path(__file__).resolve().parents[1] / 'convert_terrain_images.py'
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / 'input'
            source.mkdir()
            (source / 'TerrainHeight.OZB').write_bytes(fixture(packed_height(381) * (SIDE * SIDE)))
            (source / 'TerrainLight.OZB').write_bytes(fixture(bytes(PIXEL_BYTES)))
            output = base / 'output'
            result = subprocess.run([sys.executable, str(script), '--world', str(source), '--output', str(output),
                                     '--turbojpeg', str(LIBRARY)], capture_output=True)
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual({'converted': 2}, json.loads((output / 'manifest.json').read_text())['summary'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, required=True)
    LIBRARY = parser.parse_args().library
    unittest.main(argv=[sys.argv[0]], verbosity=2)
