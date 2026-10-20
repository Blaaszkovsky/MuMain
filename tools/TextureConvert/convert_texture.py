"""Normalize one offline image into the OZT layout understood by MuMain."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import struct

import PIL
from PIL import Image

MAX_DIMENSION = 2048
MAX_INPUT_BYTES = 32 * 1024 * 1024
CONTAINER_PREFIXES = {'ozp': 4, 'ozt': 4, 'ozj': 24}
IMAGE_FORMATS = {'ozp': ('PNG',), 'ozt': ('TGA',), 'ozj': ('JPEG', 'TGA'),
                 'png': ('PNG',), 'tga': ('TGA',), 'dds': ('DDS',)}
DDS_HEADER_BYTES = 128
DDS_BLOCK_BYTES = {b'DXT1': 8, b'DXT3': 16, b'DXT5': 16}


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def validate_dds(raw):
    if len(raw) < DDS_HEADER_BYTES or raw[:4] != b'DDS ':
        raise ValueError('Expected a decrypted DDS file, not an encrypted OZD')
    header_size, = struct.unpack_from('<I', raw, 4)
    height, width = struct.unpack_from('<II', raw, 12)
    depth, mipmaps = struct.unpack_from('<II', raw, 24)
    pixel_header_size, flags = struct.unpack_from('<II', raw, 76)
    caps2, = struct.unpack_from('<I', raw, 112)
    if header_size != 124 or pixel_header_size != 32 or depth > 1 or mipmaps > 1 or caps2:
        raise ValueError('Only a single 2D DDS surface without mipmaps is supported')
    fourcc = raw[84:88]
    if flags & 4:
        if fourcc not in DDS_BLOCK_BYTES:
            raise ValueError('Unsupported DDS compression')
        size = ((width + 3) // 4) * ((height + 3) // 4) * DDS_BLOCK_BYTES[fourcc]
    else:
        bits, = struct.unpack_from('<I', raw, 88)
        if bits not in (24, 32):
            raise ValueError('Only 24/32-bit uncompressed DDS pixels are supported')
        size = width * height * (bits // 8)
    if len(raw) != DDS_HEADER_BYTES + size:
        raise ValueError('DDS contains truncated pixels, padding, or extra surfaces')


def read_image(raw, kind):
    payload = raw[CONTAINER_PREFIXES.get(kind, 0):]
    if kind == 'dds':
        validate_dds(payload)
    if payload.startswith(b'\x89PNG\r\n\x1a\n') and len(payload) > 24 and payload[24] > 8:
        raise ValueError('High precision PNG pixels cannot be preserved in 8-bit OZT')
    with Image.open(io.BytesIO(payload)) as image:
        if image.format not in IMAGE_FORMATS[kind]:
            raise ValueError('Image content does not match the declared input format')
        if not (0 < image.width <= MAX_DIMENSION and 0 < image.height <= MAX_DIMENSION):
            raise ValueError('Image exceeds MuMain 2048x2048 limit; resampling is not allowed')
        if getattr(image, 'n_frames', 1) != 1:
            raise ValueError('Animated or multi-frame images are not supported')
        if image.mode not in ('1', 'L', 'LA', 'P', 'RGB', 'RGBA'):
            raise ValueError('Pixel mode cannot be preserved in 8-bit RGBA')
        image.load()
        return image.convert('RGBA')


def encode_ozt(image):
    # Uncompressed TGA, no image ID/palette, eight alpha bits, bottom row first.
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0,
                         image.width, image.height, 32, 8)
    with image.transpose(Image.Transpose.FLIP_TOP_BOTTOM) as bottom_first:
        pixels = bottom_first.tobytes('raw', 'BGRA')
    return bytes(4) + header + pixels


def convert(source, output, kind=None):
    source, output = Path(source), Path(output)
    kind = kind or source.suffix.lower().removeprefix('.')
    if kind not in IMAGE_FORMATS:
        raise ValueError('Supported: DDS (already decrypted), PNG, TGA, OZP, OZT, OZJ')
    with source.open('rb') as stream:
        size = os.fstat(stream.fileno()).st_size
        if size > MAX_INPUT_BYTES:
            raise ValueError('Input exceeds the texture reader 32 MiB safety limit')
        raw = stream.read(size + 1)
    if len(raw) != size:
        raise ValueError('Input size changed during reading')
    source_hash = digest(raw)
    with read_image(raw, kind) as image:
        del raw
        encoded = encode_ozt(image)
        width, height = image.size
        rgba_hash = digest(image.tobytes())
    filename = source.stem + '.OZT'
    manifest = dict(schema_version=1, operation='decoded-rgba-normalization',
                    source=str(source.resolve()), source_sha256=source_hash, input_kind=kind,
                    output=filename, output_sha256=digest(encoded),
                    rgba_sha256=rgba_hash, width=width, height=height,
                    required_reference=source.stem + '.tga', pillow_version=PIL.__version__,
                    limits=['Model references and UI/effect semantics are not modified',
                            'Source metadata is retained only in the unchanged source file'])
    # A fresh directory is the unit of output; never replace an existing file/tree.
    output.mkdir(parents=True, exist_ok=False)
    (output / filename).write_bytes(encoded)
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path, help='new directory for OZT and manifest.json')
    parser.add_argument('--format', choices=tuple(IMAGE_FORMATS), dest='kind')
    args = parser.parse_args()
    try:
        manifest = convert(args.source, args.output, args.kind)
    except (ValueError, OSError, MemoryError) as error:
        parser.exit(1, f'Conversion failed: {error}\n')
    print(json.dumps(manifest))


if __name__ == '__main__':
    main()
