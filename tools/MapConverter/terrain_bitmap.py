"""Strict, fixed-size terrain OZB parsing and exact legacy height conversion."""
from collections import Counter
import struct

from map_format import CELL_COUNT, sha256

SIDE = 256
CHANNELS = 3
OZB_PREFIX_BYTES = 4
BMP_FILE_HEADER = struct.Struct('<2sIHHI')
BMP_INFO_HEADER = struct.Struct('<IiiHHIIiiII')
BMP_HEADER_BYTES = BMP_FILE_HEADER.size + BMP_INFO_HEADER.size
BITMAP_INFO_BYTES = 40
TRUECOLOR_BITS = 24
PIXEL_BYTES = CELL_COUNT * CHANNELS
EXTENDED_OZB_BYTES = OZB_PREFIX_BYTES + BMP_HEADER_BYTES + PIXEL_BYTES
LEGACY_BMP_HEADER_BYTES = 1080
LEGACY_HEIGHT_BYTES = OZB_PREFIX_BYTES + LEGACY_BMP_HEADER_BYTES + CELL_COUNT
HEIGHT_OFFSET = -500
LEGACY_FACTOR_NUMERATOR = 3
LEGACY_FACTOR_DENOMINATOR = 2
BYTE_MAX = 255


def parse_terrain_ozb(data):
    if len(data) != EXTENDED_OZB_BYTES:
        raise ValueError('expected exactly one prefixed 256x256 uncompressed 24-bit BMP')
    signature, size, reserved1, reserved2, offset = BMP_FILE_HEADER.unpack_from(data, OZB_PREFIX_BYTES)
    fields = BMP_INFO_HEADER.unpack_from(data, OZB_PREFIX_BYTES + BMP_FILE_HEADER.size)
    header, width, height, planes, bits, compression, image_size, _, _, used, important = fields
    if (signature != b'BM' or data[:OZB_PREFIX_BYTES] != data[OZB_PREFIX_BYTES:2 * OZB_PREFIX_BYTES]
            or size != len(data) - OZB_PREFIX_BYTES or reserved1 or reserved2):
        raise ValueError('invalid OZB prefix or BMP file header')
    if (header != BITMAP_INFO_BYTES or width != SIDE or height != SIDE or planes != 1
            or bits != TRUECOLOR_BITS or compression or image_size not in (0, PIXEL_BYTES)
            or offset != BMP_HEADER_BYTES or used or important):
        raise ValueError('unsupported BMP dimensions, layout, orientation or compression')
    return data[OZB_PREFIX_BYTES + BMP_HEADER_BYTES:]


def bgr_to_rgb(pixels):
    if len(pixels) != PIXEL_BYTES:
        raise ValueError('expected exactly 65536 three-channel pixels')
    output = bytearray(pixels)
    output[0::CHANNELS] = pixels[2::CHANNELS]
    output[2::CHANNELS] = pixels[0::CHANNELS]
    return bytes(output)


def extended_heights(pixels):
    if len(pixels) != PIXEL_BYTES:
        raise ValueError('invalid packed height count')
    return [((pixels[index] << 16) | (pixels[index + 1] << 8) | pixels[index + 2]) + HEIGHT_OFFSET
            for index in range(0, PIXEL_BYTES, CHANNELS)]


def height_analysis(pixels):
    heights = extended_heights(pixels)
    failures = Counter()
    for value in heights:
        scaled = value * LEGACY_FACTOR_DENOMINATOR
        if scaled < 0:
            failures['negative'] += 1
        elif scaled > BYTE_MAX * LEGACY_FACTOR_NUMERATOR:
            failures['above_legacy_range'] += 1
        elif scaled % LEGACY_FACTOR_NUMERATOR:
            failures['off_legacy_grid'] += 1
    return dict(samples=CELL_COUNT, minimum=min(heights), maximum=max(heights),
                distinct_heights=len(set(heights)), exact_legacy_samples=CELL_COUNT - sum(failures.values()),
                nonrepresentable_samples=sum(failures.values()), failure_counts=dict(failures),
                exact_legacy_possible=not failures, packed_pixels_sha256=sha256(pixels))


def decode_legacy_height(data):
    if len(data) != LEGACY_HEIGHT_BYTES:
        raise ValueError('invalid legacy height size')
    cells = data[OZB_PREFIX_BYTES + LEGACY_BMP_HEADER_BYTES:]
    return [value * LEGACY_FACTOR_NUMERATOR / LEGACY_FACTOR_DENOMINATOR for value in cells]


def convert_height(pixels):
    analysis = height_analysis(pixels)
    if not analysis['exact_legacy_possible']:
        raise ValueError('packed heights cannot be represented exactly as byte * 1.5; select extended loader')
    heights = extended_heights(pixels)
    cells = bytes(value * LEGACY_FACTOR_DENOMINATOR // LEGACY_FACTOR_NUMERATOR for value in heights)
    bmp_size = LEGACY_BMP_HEADER_BYTES + CELL_COUNT
    file_header = BMP_FILE_HEADER.pack(b'BM', bmp_size, 0, 0, LEGACY_BMP_HEADER_BYTES)
    info = BMP_INFO_HEADER.pack(BITMAP_INFO_BYTES, SIDE, SIDE, 1, 8, 0, CELL_COUNT, 0, 0, 256, 256)
    palette = b''.join(bytes((value, value, value, 0)) for value in range(256))
    header = (file_header + info + palette).ljust(LEGACY_BMP_HEADER_BYTES, b'\0')
    output = header[:OZB_PREFIX_BYTES] + header + cells
    if decode_legacy_height(output) != heights:
        raise ValueError('legacy height roundtrip mismatch')
    return output
