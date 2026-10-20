"""Convert one world's terrain light exactly; refuse unrepresentable legacy heights."""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess

from lossless_jpeg import LosslessJpegCodec
from map_format import sha256
from terrain_bitmap import (EXTENDED_OZB_BYTES, OZB_PREFIX_BYTES, BMP_HEADER_BYTES,
                            bgr_to_rgb, convert_height, height_analysis, parse_terrain_ozb)


def source_data(path):
    with path.open('rb') as stream:
        data = stream.read(EXTENDED_OZB_BYTES + 1)
    if len(data) != EXTENDED_OZB_BYTES:
        raise ValueError('terrain source exceeds or differs from the fixed 196666-byte layout')
    return data


def convert_one(path, destination, codec):
    entry = dict(source_name=path.name, status='error')
    try:
        data = source_data(path)
        entry.update(source_bytes=len(data), source_sha256=sha256(data))
        pixels = parse_terrain_ozb(data)
        if path.name == 'TerrainHeight.OZB':
            entry.update(height_analysis(pixels))
            if not entry['exact_legacy_possible']:
                entry.update(status='blocked', reason='legacy height = byte * 1.5 cannot preserve these values',
                             minimal_loader_change='select existing OpenTerrainHeightNew for logical map 112; retain -500 offset')
                return entry
            output, name = convert_height(pixels), path.name
        else:
            if codec is None:
                raise ValueError('lossless terrain light requires --turbojpeg with TurboJPEG 3 support')
            expected = bgr_to_rgb(pixels)
            output, name = codec.encode(expected), 'TerrainLight.OZJ'
            decoded = codec.decode_like_mumain(output)
            header = data[:OZB_PREFIX_BYTES + BMP_HEADER_BYTES]
            if decoded != expected or header + bgr_to_rgb(decoded) != data:
                raise ValueError('legacy API pixel or source OZB reconstruction mismatch')
            entry.update(source_header_hex=header.hex(), rgb_bottom_up_sha256=sha256(expected),
                         legacy_api_rgb_sha256=sha256(decoded), unequal_channels=0,
                         source_ozb_reconstruction_verified=True, jpeg_mode='SOF3 RGB 8-bit predictor1 point-transform0')
        with (destination / name).open('xb') as stream:
            stream.write(output)
        if (destination / name).read_bytes() != output or source_data(path) != data:
            raise ValueError('disk verification or source preservation failed')
        entry.update(status='converted', output_name=name, output_bytes=len(output),
                     output_sha256=sha256(output), roundtrip_verified=True)
    except (OSError, ValueError) as error:
        entry['reason'] = str(error)
    return entry


def provenance(codec):
    directory = Path(__file__).resolve().parent
    result = dict(source_revision=subprocess.check_output(
        ['git', '-C', str(directory), 'rev-parse', 'HEAD'], text=True).strip())
    result['scripts'] = {name: sha256((directory / name).read_bytes()) for name in
                         ('convert_terrain_images.py', 'terrain_bitmap.py', 'lossless_jpeg.py')}
    if codec:
        result.update(turbojpeg_library=str(codec.library_path),
                      turbojpeg_sha256=sha256(codec.library_path.read_bytes()))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--world', type=Path, required=True, help='one World directory; no recursive scan')
    parser.add_argument('--output', type=Path, required=True, help='fresh external directory')
    parser.add_argument('--turbojpeg', type=Path, help='existing trusted TurboJPEG 3 DLL/shared library')
    args = parser.parse_args()
    try:
        world = args.world.resolve(strict=True)
        output = args.output.resolve()
        if not world.is_dir() or output.exists() or output.is_relative_to(world) or world.is_relative_to(output):
            raise ValueError('require a world directory and a fresh disjoint output directory')
        paths = [world / name for name in ('TerrainHeight.OZB', 'TerrainLight.OZB')]
        if any(not path.resolve(strict=True).is_relative_to(world) for path in paths):
            raise ValueError('terrain source symlink escapes world directory')
        codec = LosslessJpegCodec(args.turbojpeg) if args.turbojpeg else None
        identity = provenance(codec)
        output.mkdir(parents=True, exist_ok=False)
        entries = [convert_one(path, output, codec) for path in paths]
        manifest = dict(schema_version=1, input_world=str(world), provenance=identity, runtime_tested=False,
                        summary=dict(Counter(entry['status'] for entry in entries)), files=entries)
        (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(manifest['summary'], sort_keys=True))
        print(output / 'manifest.json')
        return 1 if any(entry['status'] != 'converted' for entry in entries) else 0
    except (OSError, ValueError, AttributeError, subprocess.SubprocessError) as error:
        parser.exit(2, str(error) + '\n')


if __name__ == '__main__':
    raise SystemExit(main())
