"""Build a private diagnostic atlas of Ferea data; never claim a game render."""
import argparse
import colorsys
import io
import json
from pathlib import Path
import struct
import subprocess

from ferea_audit import SIDE, WORLD_UNITS_PER_CELL, audit_objects, audit_tiles
from map_format import CELL_COUNT, decode, legacy_decrypt, restore_objects, sha256
from terrain_bitmap import (EXTENDED_OZB_BYTES, bgr_to_rgb, extended_heights, parse_terrain_ozb)

MAX_INPUT_BYTES = 2 * 1024 * 1024
PANEL_SCALE = 2
TITLE_HEIGHT = 30
FOOTER_HEIGHT = 95


def read_bounded(path, maximum=MAX_INPUT_BYTES):
    with path.open('rb') as stream:
        data = stream.read(maximum + 1)
    if len(data) > maximum:
        raise ValueError('input exceeds preview bound: ' + str(path))
    return data


def load_inputs(args):
    manifest = json.loads(read_bounded(args.manifest))
    entries = {entry['path']: entry for entry in manifest['files']}
    payloads, inputs = {}, {}
    for kind in ('att', 'map', 'obj'):
        name = f'EncTerrain113.{kind}'
        original = read_bounded(args.world / name)
        converted = read_bounded(args.converted / name)
        entry = entries['World113/' + name]
        if sha256(original) != entry['source_sha256'] or sha256(converted) != entry['output_sha256']:
            raise ValueError('map source/output does not match the reviewed manifest: ' + name)
        payloads[kind] = decode(converted, kind, None)[0]
        inputs[name] = dict(source_sha256=sha256(original), converted_sha256=sha256(converted))
    source_obj = legacy_decrypt(read_bounded(args.world / 'EncTerrain113.obj'))
    sidecar_bytes = read_bounded(args.converted / 'EncTerrain113.obj.lighting.json')
    if sha256(sidecar_bytes) != entries['World113/EncTerrain113.obj']['sidecar_sha256']:
        raise ValueError('lighting sidecar differs from manifest')
    if restore_objects(payloads['obj'], json.loads(sidecar_bytes)) != source_obj:
        raise ValueError('lighting flags do not reconstruct the source OBJ')
    return payloads, source_obj, inputs


def load_light(args):
    from PIL import Image
    source = read_bounded(args.world / 'TerrainLight.OZB', EXTENDED_OZB_BYTES)
    converted = read_bounded(args.light)
    pixels = parse_terrain_ozb(source)
    with Image.open(io.BytesIO(source[4:])) as original_image, Image.open(io.BytesIO(converted[24:])) as converted_image:
        if original_image.size != (SIDE, SIDE) or converted_image.size != original_image.size:
            raise ValueError('unexpected light dimensions')
        if original_image.convert('RGB').tobytes() != converted_image.convert('RGB').tobytes():
            raise ValueError('light conversion has pixel/orientation differences')
    return bgr_to_rgb(pixels), dict(source_sha256=sha256(source),
            converted_sha256=sha256(converted), unequal_channels=0)


def slot_color(slot):
    return tuple(round(channel * 255) for channel in colorsys.hsv_to_rgb((slot * 0.61803398875) % 1, 0.65, 0.85))


def make_atlas(payloads, heights, light, objects):
    from PIL import Image, ImageDraw
    height_min, height_max = min(heights), max(heights)
    height_range = max(1, height_max - height_min)
    height_pixels = bytes(channel for value in heights for channel in
                          (round((value - height_min) * 255 / height_range),
                           round((value - height_min) * 180 / height_range), 90))
    layer1 = payloads['map'][2:2 + CELL_COUNT]
    alpha = payloads['map'][2 + CELL_COUNT * 2:]
    walls = struct.unpack_from('<65536H', payloads['att'], 4)
    tile_pixels = bytes(channel for slot in layer1 for channel in slot_color(slot))
    wall_pixels = bytes(channel for value in walls for channel in
                        ((190, 60, 60) if value & 4 else (90, 100, 180) if value & 8 else
                         (80, 190, 90) if value & 1 else (35, 40, 45)))
    panels = [(f'Height {height_min}..{height_max} (stored rows)', Image.frombytes('RGB', (SIDE, SIDE), height_pixels)),
              ('Source layer1 slot colors (not materials)', Image.frombytes('RGB', (SIDE, SIDE), tile_pixels)),
              ('Source vertex blend alpha', Image.frombytes('L', (SIDE, SIDE), alpha).convert('RGB')),
              ('Raw collision: red block / green safe', Image.frombytes('RGB', (SIDE, SIDE), wall_pixels)),
              ('Objects: blue/yellow flag pairs; red type160', object_panel(objects)),
              ('Lossless light RGB (stored rows)', Image.frombytes('RGB', (SIDE, SIDE), light))]
    panel_size = SIDE * PANEL_SCALE
    atlas = Image.new('RGB', (panel_size * 3, (panel_size + TITLE_HEIGHT) * 2 + FOOTER_HEIGHT), '#10151d')
    draw = ImageDraw.Draw(atlas)
    for index, (title, image) in enumerate(panels):
        x, y = index % 3 * panel_size, index // 3 * (panel_size + TITLE_HEIGHT)
        draw.text((x + 8, y + 8), title, fill='white')
        atlas.paste(image.resize((panel_size, panel_size), Image.Resampling.NEAREST), (x, y + TITLE_HEIGHT))
    draw.text((12, atlas.height - FOOTER_HEIGHT + 12),
              'FEREA / logical112 / World113 — DATA PREVIEW ONLY\n'
              'No mesh, animation or S16 shader render. Lighting flags retained as data.\n'
              'Missing generic tile24/26 bindings and global model slot160 remain unresolved.', fill='#ffbf70')
    return atlas


def object_panel(objects):
    from PIL import Image, ImageDraw
    image = Image.new('RGB', (SIDE, SIDE), '#15202b')
    draw = ImageDraw.Draw(image)
    for record in sorted(objects['records'], key=lambda item: item['slot_conflict']):
        x, y = (value / WORLD_UNITS_PER_CELL for value in record['position'][:2])
        color = '#79bfff' if record['lighting_flags'] == [0, 1] else '#ffd16a'
        if record['slot_conflict']:
            draw.ellipse((x - 2, y - 2, x + 2, y + 2), outline='#ff4040')
        else:
            draw.point((x, y), fill=color)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('world', 'converted', 'manifest', 'light', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        args.world = args.world.resolve(strict=True)
        args.output = args.output.resolve()
        if args.world.name != 'World113' or args.output.exists() or args.output.is_relative_to(args.world):
            raise ValueError('require source World113 and a fresh external output directory')
        payloads, original_obj, inputs = load_inputs(args)
        height_source = read_bounded(args.world / 'TerrainHeight.OZB', EXTENDED_OZB_BYTES)
        heights = extended_heights(parse_terrain_ozb(height_source))
        light, light_evidence = load_light(args)
        objects = audit_objects(original_obj)
        tiles = audit_tiles(payloads['map'], [path.name for path in args.world.iterdir() if path.is_file()])
        atlas = make_atlas(payloads, heights, light, objects)
        args.output.mkdir(parents=True, exist_ok=False)
        atlas.save(args.output / 'ferea-data-preview.png')
        (args.output / 'objects-with-lighting.json').write_text(json.dumps(objects, indent=2) + '\n', encoding='utf-8')
        evidence = dict(source_revision=subprocess.check_output(['git', '-C', str(Path(__file__).parent),
                       'rev-parse', 'HEAD'], text=True).strip(), inputs=inputs, height_sha256=sha256(height_source),
                       light=light_evidence, tiles=tiles, object_count=objects['count'],
                       slot_conflicts=objects['slot_conflict_count'], lighting_flags_retained=True,
                       preview='data atlas and object markers only', game_render_acceptance=False,
                       blockers=['S16 tile24/26 material bindings unverified',
                                 'raw object160 aliases global MODEL_LOGO; no guessed remap applied',
                                 'OBJv1 lighting byte semantics unverified; source flags preserved explicitly',
                                 'full client build and visual test not authorized/run'])
        (args.output / 'manifest.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
        print(args.output / 'ferea-data-preview.png')
        print(f"{objects['count']} source objects retained; full game rendering remains blocked")
        return 0
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(2, str(error) + '\n')


if __name__ == '__main__':
    raise SystemExit(main())
