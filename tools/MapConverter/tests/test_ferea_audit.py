from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ferea_audit import CELL_COUNT, SIDE, audit_objects, audit_tiles, generic_tile_file, quad_slots


class FereaAuditTests(unittest.TestCase):
    def test_material_slot_names_match_generic_loader(self):
        self.assertEqual('ExtTile11.OZJ', generic_tile_file(24))
        self.assertEqual('ExtTile13.OZJ', generic_tile_file(26))
        self.assertEqual('TileRock04.OZJ', generic_tile_file(10))
        self.assertIsNone(generic_tile_file(255))

    def test_neighbor_alpha_uses_layer2_even_when_anchor_alpha_is_zero(self):
        layer1, layer2, alpha = bytearray(CELL_COUNT), bytearray(CELL_COUNT), bytearray(CELL_COUNT)
        layer2[0] = 26
        alpha[SIDE + 1] = 51
        base, blend, corners = quad_slots(layer1, layer2, alpha, 0, 0)
        self.assertEqual((0, 26, (0, 0, 51, 0)), (base, blend, corners))

    def test_fully_opaque_layer2_and_absent_overlay(self):
        layer1, layer2 = bytes([3]) * CELL_COUNT, bytes([8]) * CELL_COUNT
        self.assertEqual((8, None), quad_slots(layer1, layer2, bytes([255]) * CELL_COUNT, 0, 0)[:2])
        self.assertEqual((3, None), quad_slots(layer1, layer2, bytes(CELL_COUNT), 0, 0)[:2])

    def test_quad_corner_lookup_wraps_the_256_cell_boundary(self):
        layer1, layer2, alpha = bytearray(CELL_COUNT), bytearray(CELL_COUNT), bytearray(CELL_COUNT)
        layer2[-1] = 26
        alpha[0] = 51
        self.assertEqual((0, 26, (0, 0, 51, 0)), quad_slots(layer1, layer2, alpha, 255, 255))

    def test_missing_tile_is_reported_without_substitution(self):
        payload = b'\0\x71' + bytes([24]) * CELL_COUNT + bytes([255]) * CELL_COUNT + bytes(CELL_COUNT)
        result = audit_tiles(payload, ['ExtTile10.OZJ'])
        self.assertEqual('ExtTile11.OZJ', result['bindings'][0]['filename'])
        self.assertFalse(result['bindings'][0]['available'])
        self.assertEqual(CELL_COUNT, len(result['missing_cells']))

    def test_raw_flags_type160_and_transform_are_preserved(self):
        payload = b'\1\x71\1\0' + struct.pack('<h7fBB', 160, 100, 200, -50, 0, 90, 180, 1.25, 0, 1)
        result = audit_objects(payload)
        record = result['records'][0]
        self.assertEqual([0, 1], record['lighting_flags'])
        self.assertEqual(160, record['source_type'])
        self.assertEqual('Object161.bmd', record['model_file'])
        self.assertEqual([100, 200, -50], record['position'])
        self.assertEqual(1, result['slot_conflict_count'])

    def test_wrong_world_and_wrong_obj_version_are_refused(self):
        with self.assertRaises(ValueError):
            audit_objects(b'\0\x71\0\0')
        with self.assertRaises(ValueError):
            audit_tiles(b'\0\x70' + bytes(CELL_COUNT * 3), [])


if __name__ == '__main__':
    unittest.main()
