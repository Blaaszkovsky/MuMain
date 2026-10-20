"""Ferea-only data inspection without guessed material or lighting semantics."""
from collections import Counter

from map_format import CELL_COUNT, OBJECT_HEADER, OBJECT_RECORD, OBJECT_STRIDES, inspect_objects

SIDE = 256
MAP_HEADER_BYTES = 2
WORLD_OBJECT_SLOTS = 160
WORLD_UNITS_PER_CELL = 100
WORLD_BYTE = 113
LEGACY_TILES = ('TileGrass01', 'TileGrass02', 'TileGround01', 'TileGround02', 'TileGround03',
                'TileWater01', 'TileWood01', 'TileRock01', 'TileRock02', 'TileRock03',
                'TileRock04', 'TileRock05', 'TileRock06', 'TileRock07')


def generic_tile_file(slot):
    if 0 <= slot < len(LEGACY_TILES):
        return LEGACY_TILES[slot] + '.OZJ'
    if len(LEGACY_TILES) <= slot < 30:
        return f'ExtTile{slot - len(LEGACY_TILES) + 1:02}.OZJ'
    return None


def quad_slots(layer1, layer2, alpha, x, y):
    index = y * SIDE + x
    corners = (index, y * SIDE + ((x + 1) % SIDE),
               ((y + 1) % SIDE) * SIDE + ((x + 1) % SIDE), ((y + 1) % SIDE) * SIDE + x)
    values = tuple(alpha[corner] for corner in corners)
    full = all(value == 255 for value in values)
    base = layer2[index] if full else layer1[index]
    blend = layer2[index] if any(values) and not full and layer2[index] != 255 else None
    return base, blend, values


def audit_tiles(payload, available_files):
    if len(payload) != MAP_HEADER_BYTES + CELL_COUNT * 3 or payload[:2] != bytes((0, WORLD_BYTE)):
        raise ValueError('expected Ferea legacy MAP payload')
    layer1, layer2, alpha = (payload[MAP_HEADER_BYTES + i * CELL_COUNT:MAP_HEADER_BYTES + (i + 1) * CELL_COUNT]
                             for i in range(3))
    base_counts, blend_counts, missing_cells = Counter(), Counter(), []
    available = {name.casefold() for name in available_files}
    for y in range(SIDE):
        for x in range(SIDE):
            base, blend, corners = quad_slots(layer1, layer2, alpha, x, y)
            base_counts[base] += 1
            if blend is not None:
                blend_counts[blend] += 1
            missing = [slot for slot in (base, blend) if slot is not None and
                       (generic_tile_file(slot) or '').casefold() not in available]
            if missing:
                missing_cells.append(dict(x=x, y=y, base=base, blend=blend, alpha=list(corners), missing=missing))
    slots = sorted(set(base_counts) | set(blend_counts))
    bindings = [dict(slot=slot, filename=generic_tile_file(slot), base_quads=base_counts[slot],
                     blend_quads=blend_counts[slot], available=(generic_tile_file(slot) or '').casefold() in available)
                for slot in slots]
    return dict(binding_model='current generic MuMain slots, not verified S16 bindings',
                visibility='LOD1 candidate quads before frustum/terrain-wall/render-specific suppression',
                bindings=bindings, missing_cells=missing_cells)


def audit_objects(payload):
    metadata = inspect_objects(payload)
    if metadata['version'] != 1 or metadata['world_byte'] != WORLD_BYTE:
        raise ValueError('expected Ferea OBJ v1 with world byte113')
    records = []
    for index in range(metadata['count']):
        offset = OBJECT_HEADER.size + index * OBJECT_STRIDES[1]
        values = OBJECT_RECORD.unpack_from(payload, offset)
        flags = list(payload[offset + OBJECT_RECORD.size:offset + OBJECT_STRIDES[1]])
        records.append(dict(index=index, source_type=values[0], model_file=f'Object{values[0] + 1:02}.bmd',
                            position=list(values[1:4]), angle=list(values[4:7]), scale=values[7],
                            lighting_flags=flags, slot_conflict=values[0] >= WORLD_OBJECT_SLOTS))
    used = set(metadata['object_types'])
    return dict(count=metadata['count'], light_flag_pairs=metadata['light_flag_pairs'],
                flag_semantics='unknown; neither byte is mapped onto MuMain LightEnable by this tool',
                unused_legacy_slots=[slot for slot in range(WORLD_OBJECT_SLOTS) if slot not in used],
                slot_conflict_count=sum(record['slot_conflict'] for record in records), records=records)
