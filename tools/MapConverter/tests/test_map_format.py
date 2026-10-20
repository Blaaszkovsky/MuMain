import copy
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from map_format import (ATT_BYTES, CELL_COUNT, MAP_BYTES, OBJECT_RECORD, bux, decode,
                        downgrade_objects, encode_legacy, inspect_payload,
                        legacy_decrypt, legacy_encrypt, restore_objects)


def objects(version=1, count=2):
    record = OBJECT_RECORD.pack(17, 100.5, 20, -200, 0, 90, 180, 1.25)
    extras = (b'', b'\0\1', b'\0\1\1', b'\0\1\1' + struct.pack('<3f', 1, 1, 1))[version]
    return struct.pack('<BBh', version, 113, count) + (record + extras) * count


class MapFormatTests(unittest.TestCase):
    def test_legacy_roundtrip_covers_all_byte_values(self):
        data = bytes(range(256)) * 3
        self.assertEqual(data, legacy_decrypt(legacy_encrypt(data)))
        self.assertEqual(data, bux(bux(data)))

    def test_att_both_cell_widths(self):
        for size in ATT_BYTES:
            payload = b'\0\x71\xff\xff' + bytes(size - 4)
            self.assertTrue(inspect_payload(payload, 'att')['supported'])
            self.assertEqual(payload, decode(encode_legacy(payload, 'att'), 'att', None)[0])

    def test_att_rejects_runtime_attribute_loss(self):
        payload = b'\0\x71\xff\xff' + struct.pack('<H', 256) + bytes(CELL_COUNT * 2 - 2)
        info = inspect_payload(payload, 'att')
        self.assertFalse(info['supported'])
        self.assertEqual(1, info['high_flag_cells'])
        payload = b'\0\x71\xff\xff' + b'\x80' + bytes(CELL_COUNT - 1)
        self.assertFalse(inspect_payload(payload, 'att')['supported'])

    def test_map_preserves_three_layers(self):
        payload = b'\0\x71' + bytes(range(256)) * (CELL_COUNT * 3 // 256)
        self.assertEqual(MAP_BYTES, len(payload))
        self.assertEqual(payload, decode(encode_legacy(payload, 'map'), 'map', None)[0])
        self.assertEqual(3, len(inspect_payload(payload, 'map')['layer_sha256']))

    def test_exact_eof_and_headers_required(self):
        fixtures = [('att', b'\0\x71\xff\xff' + bytes(CELL_COUNT)),
                    ('map', b'\0\x71' + bytes(CELL_COUNT * 3)), ('obj', objects())]
        for kind, payload in fixtures:
            for invalid in (payload[:-1], payload + b'\0', b'\xff' + payload[1:]):
                with self.subTest(kind=kind, length=len(invalid)), self.assertRaises(ValueError):
                    inspect_payload(invalid, kind)

    def test_object_v1_geometry_is_byte_identical_and_flags_recover(self):
        original = objects()
        target, sidecar = downgrade_objects(original)
        self.assertEqual(4 + 30 * 2, len(target))
        self.assertEqual(original[4:34], target[4:34])
        self.assertEqual(original[36:66], target[34:64])
        self.assertEqual('00010001', sidecar['light_flags_hex'])
        self.assertEqual(original, restore_objects(target, sidecar))
        self.assertTrue(inspect_payload(original, 'obj')['runtime_losses'])

    def test_sidecar_rejects_damage_and_wrong_target(self):
        target, sidecar = downgrade_objects(objects())
        for key, value in [('light_flags_hex', '0000'), ('source_payload_sha256', '0' * 64),
                           ('target_payload_sha256', '0' * 64), ('count', 3), ('schema_version', 2)]:
            damaged = copy.deepcopy(sidecar)
            damaged[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                restore_objects(target, damaged)

    def test_v0_passthrough_and_newer_versions_withheld(self):
        self.assertEqual((objects(0), None), downgrade_objects(objects(0)))
        for version in (2, 3):
            self.assertFalse(inspect_payload(objects(version), 'obj')['supported'])
            with self.assertRaises(ValueError):
                downgrade_objects(objects(version))

    def test_rejects_negative_count_negative_type_and_nan(self):
        with self.assertRaises(ValueError):
            inspect_payload(b'\0\x71\xff\xff', 'obj')
        for offset, replacement in [(4, struct.pack('<h', -1)), (6, struct.pack('<f', math.nan))]:
            payload = bytearray(objects())
            payload[offset:offset + len(replacement)] = replacement
            with self.assertRaises(ValueError):
                inspect_payload(payload, 'obj')

    def test_unknown_and_truncated_envelopes_rejected(self):
        for kind in ('att', 'map'):
            for data in (kind.upper().encode() + b'\2', kind.upper().encode() + b'\1' + bytes(10)):
                with self.assertRaises(ValueError):
                    decode(data, kind, None)


class CommandTests(unittest.TestCase):
    def test_lighting_opt_in_never_enables_attribute_loss(self):
        script = Path(__file__).resolve().parents[1] / 'convert_maps.py'
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / 'input'
            source.mkdir()
            payload = b'\0\x71\xff\xff' + struct.pack('<H', 256) + bytes(CELL_COUNT * 2 - 2)
            (source / 'test.att').write_bytes(encode_legacy(payload, 'att'))
            output = base / 'output'
            result = subprocess.run([sys.executable, str(script), '--input', str(source),
                                     '--output', str(output), '--convert', '--allow-lighting-loss'],
                                    capture_output=True)
            self.assertEqual(1, result.returncode)
            manifest = json.loads((output / 'manifest.json').read_text())
            self.assertEqual({'unsupported': 1}, manifest['summary'])
            self.assertFalse((output / 'test.att').exists())

    def test_cli_reports_malformed_input_and_header_world_mismatch(self):
        script = Path(__file__).resolve().parents[1] / 'convert_maps.py'
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / 'World114'
            source.mkdir()
            (source / 'broken.obj').write_bytes(encode_legacy(b'\0', 'obj'))
            (source / 'wrong.obj').write_bytes(encode_legacy(objects(0), 'obj'))
            output = base / 'output'
            result = subprocess.run([sys.executable, str(script), '--input', str(source),
                                     '--output', str(output), '--convert'], capture_output=True)
            self.assertEqual(1, result.returncode)
            manifest = json.loads((output / 'manifest.json').read_text())
            self.assertEqual({'error': 1, 'unsupported': 1}, manifest['summary'])
            self.assertTrue(all('source_sha256' in entry for entry in manifest['files']))
            self.assertFalse(list(output.glob('*.obj')))

    def test_cli_limits_loss_preserves_source_and_refuses_existing_output(self):
        script = Path(__file__).resolve().parents[1] / 'convert_maps.py'
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / 'input'
            source.mkdir()
            raw = encode_legacy(objects(), 'obj')
            path = source / 'test.obj'
            path.write_bytes(raw)
            command = [sys.executable, str(script), '--input', str(source), '--convert']
            result = subprocess.run(command + ['--output', str(base / 'refused')], capture_output=True)
            self.assertEqual(1, result.returncode)
            manifest = json.loads((base / 'refused/manifest.json').read_text())
            self.assertEqual({'withheld': 1}, manifest['summary'])
            output = base / 'limited'
            result = subprocess.run(command + ['--output', str(output), '--allow-lighting-loss'], capture_output=True)
            self.assertEqual(0, result.returncode, result.stderr)
            converted = legacy_decrypt((output / 'test.obj').read_bytes())
            sidecar = json.loads((output / 'test.obj.lighting.json').read_text())
            self.assertEqual(objects(), restore_objects(converted, sidecar))
            self.assertEqual(raw, path.read_bytes())
            result = subprocess.run(command + ['--output', str(output)], capture_output=True)
            self.assertEqual(2, result.returncode)
            result = subprocess.run(command + ['--output', str(source / 'nested')], capture_output=True)
            self.assertEqual(2, result.returncode)


if __name__ == '__main__':
    unittest.main()
