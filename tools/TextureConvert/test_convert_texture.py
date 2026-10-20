import hashlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest

from PIL import Image

import convert_texture


class TextureConversionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def source(self, extension, format='PNG', mode='RGBA'):
        image = Image.new(mode, (2, 2))
        image.putdata([(255, 0, 0, 10), (0, 255, 0, 20),
                       (0, 0, 255, 30), (255, 255, 255, 40)] if mode == 'RGBA' else
                      [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)])
        buffer = io.BytesIO()
        image.save(buffer, format=format)
        prefix = bytes(24 if extension == '.ozj' else 4) if extension.startswith('.oz') else b''
        path = self.root / ('sample' + extension)
        path.write_bytes(prefix + buffer.getvalue())
        return path

    def test_ozp_preserves_alpha_and_bottom_first_rows_for_client(self):
        source = self.source('.ozp')
        original = source.read_bytes()
        convert_texture.convert(source, self.root / 'output')
        output = (self.root / 'output/sample.OZT').read_bytes()
        self.assertEqual(output[20:22], bytes([32, 8]))
        self.assertEqual(output[22:], bytes([255, 0, 0, 30, 255, 255, 255, 40,
                                            0, 0, 255, 10, 0, 255, 0, 20]))
        self.assertEqual(source.read_bytes(), original)
        manifest = json.loads((self.root / 'output/manifest.json').read_text())
        self.assertEqual(manifest['source_sha256'], hashlib.sha256(original).hexdigest())
        self.assertEqual(manifest['required_reference'], 'sample.tga')

    def test_24bit_ozt_adds_opaque_alpha(self):
        source = self.source('.ozt', 'TGA', 'RGB')
        convert_texture.convert(source, self.root / 'output')
        raw = (self.root / 'output/sample.OZT').read_bytes()
        self.assertEqual(list(raw[25::4]), [255, 255, 255, 255])

    def test_dds_requires_explicit_format_for_decrypted_ozd(self):
        source = self.source('.dds', 'DDS')
        renamed = source.with_suffix('.ozd')
        source.rename(renamed)
        with self.assertRaises(ValueError):
            convert_texture.convert(renamed, self.root / 'refused')
        self.assertFalse((self.root / 'refused').exists())
        convert_texture.convert(renamed, self.root / 'output', 'dds')
        self.assertEqual((self.root / 'output/sample.OZT').stat().st_size, 38)

    def test_refuses_mipmap_payload_instead_of_discarding_it(self):
        source = self.source('.dds', 'DDS')
        raw = bytearray(source.read_bytes())
        struct.pack_into('<I', raw, 28, 2)
        source.write_bytes(raw)
        with self.assertRaises(ValueError):
            convert_texture.convert(source, self.root / 'output')
        self.assertFalse((self.root / 'output').exists())

    def test_refuses_oversized_image_without_resampling(self):
        source = self.root / 'wide.png'
        Image.new('RGBA', (1025, 1)).save(source)
        with self.assertRaises(ValueError):
            convert_texture.convert(source, self.root / 'output')
        self.assertFalse((self.root / 'output').exists())

    def test_refuses_existing_output_directory(self):
        source = self.source('.ozp')
        output = self.root / 'output'
        output.mkdir()
        sentinel = output / 'sample.OZT'
        sentinel.write_bytes(b'keep')
        with self.assertRaises(FileExistsError):
            convert_texture.convert(source, output)
        self.assertEqual(sentinel.read_bytes(), b'keep')

    def test_refuses_truncated_image(self):
        source = self.source('.ozt', 'TGA')
        source.write_bytes(source.read_bytes()[:24])
        with self.assertRaises((ValueError, OSError)):
            convert_texture.convert(source, self.root / 'output')
        self.assertFalse((self.root / 'output').exists())

    def test_top_origin_rle_tga_is_normalized(self):
        source = self.source('.ozt', 'TGA')
        expected = bytes([255, 0, 0, 10, 0, 255, 0, 20,
                          0, 0, 255, 30, 255, 255, 255, 40])
        with Image.open(io.BytesIO(source.read_bytes()[4:])) as image:
            buffer = io.BytesIO()
            image.save(buffer, format='TGA', compression='tga_rle', orientation=1)
        source.write_bytes(bytes(4) + buffer.getvalue())
        convert_texture.convert(source, self.root / 'output')
        with Image.open(io.BytesIO((self.root / 'output/sample.OZT').read_bytes()[4:])) as image:
            self.assertEqual(image.tobytes(), expected)

    def test_dds_extra_surface_is_refused(self):
        source = self.source('.dds', 'DDS')
        source.write_bytes(source.read_bytes() + bytes(16))
        with self.assertRaises(ValueError):
            convert_texture.convert(source, self.root / 'output')
        self.assertFalse((self.root / 'output').exists())

    def test_high_precision_png_is_refused_instead_of_quantized(self):
        source = self.root / 'height.png'
        Image.new('I;16', (2, 2), 1000).save(source)
        with self.assertRaises(ValueError):
            convert_texture.convert(source, self.root / 'output')
        self.assertFalse((self.root / 'output').exists())


if __name__ == '__main__':
    unittest.main()
