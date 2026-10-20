"""Read two real height fixtures and record independent full-sample reference CRCs."""
import argparse
import array
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

CELLS = 256 * 256
FEREA_SHA256 = 'c6eef25b01cc43e00f6f366abdf36150baf69efa2dff4ad6e82432c594a49a94'
LORENCIA_SHA256 = '85067a70679143a387b37069b859712f6a587e707a5e875c915413cfe47c7004'


def read_fixture(path, size, expected_hash):
    with path.open('rb') as stream:
        data = stream.read(size + 1)
    if len(data) != size or hashlib.sha256(data).hexdigest() != expected_hash:
        raise ValueError('fixture size/hash mismatch: ' + str(path))
    return data


def crc32(values):
    packed = array.array('f', values)
    if packed.itemsize != 4:
        raise ValueError('preflight requires IEEE754 32-bit float storage')
    if sys.byteorder != 'little':
        packed.byteswap()
    return f'{zlib.crc32(packed.tobytes()):08x}'


def summarize(path, raw, heights):
    return dict(path=str(path.resolve()), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest(),
                samples=len(heights), minimum=min(heights), maximum=max(heights),
                negative_samples=sum(value < 0 for value in heights),
                little_endian_float32_crc32=crc32(heights))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ferea', required=True, type=Path)
    parser.add_argument('--legacy', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    ferea = read_fixture(args.ferea, 58 + CELLS * 3, FEREA_SHA256)
    legacy = read_fixture(args.legacy, 1084 + CELLS, LORENCIA_SHA256)
    if struct.unpack_from('<iiHH', ferea, 22) != (256, 256, 1, 24):
        raise ValueError('unexpected Ferea bitmap header')
    if struct.unpack_from('<iiHH', legacy, 22) != (256, 256, 1, 8):
        raise ValueError('unexpected Lorencia bitmap header')
    packed = [float(int.from_bytes(ferea[index:index + 3], 'big') - 500)
              for index in range(58, len(ferea), 3)]
    # Match the existing reader's fixed1080-byte BMP header, including its historic2-byte offset.
    old = [value * 1.5 for value in legacy[1084:]]
    login = [value * 3.0 for value in legacy[1084:]]
    result = dict(ferea=summarize(args.ferea, ferea, packed), lorencia=summarize(args.legacy, legacy, old),
                  login_factor_float32_crc32=crc32(login), native_test_run=False,
                  purpose='independent fixture/reference evidence; not execution of the C++ loader')
    with args.report.open('x', encoding='utf-8') as output:
        output.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
