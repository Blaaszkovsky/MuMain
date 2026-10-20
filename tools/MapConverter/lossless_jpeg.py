"""Bounded RGB lossless JPEG using TurboJPEG 3 and MuMain's legacy decode API."""
import ctypes as c
from pathlib import Path

from terrain_bitmap import SIDE, PIXEL_BYTES

JPEG_CAPACITY = 1024 * 1024
OZJ_PREFIX_BYTES = 24
TJINIT_COMPRESS = 0
TJPF_RGB = 0
TJCS_RGB = 0
TJFLAG_BOTTOMUP = 2
# Stable TurboJPEG 3.0 enum values, matching MuMain src/dependencies/include/turbojpeg.h.
TJPARAM_BOTTOMUP = 1
TJPARAM_NOREALLOC = 2
TJPARAM_COLORSPACE = 8
TJPARAM_LOSSLESS = 15
TJPARAM_LOSSLESSPSV = 16
TJPARAM_LOSSLESSPT = 17
BYTE_POINTER = c.POINTER(c.c_ubyte)
SOF_LOSSLESS = 0xC3
START_OF_SCAN = 0xDA


def validate_lossless_headers(jpeg):
    """Require 8-bit RGB SOF3, predictor 1 and point transform 0, not quality-100 DCT."""
    if not jpeg.startswith(b'\xff\xd8') or not jpeg.endswith(b'\xff\xd9'):
        raise ValueError('missing JPEG start/end marker')
    offset, frame_seen = 2, False
    while offset + 4 <= len(jpeg):
        if jpeg[offset] != 255:
            raise ValueError('invalid JPEG header marker')
        marker = jpeg[offset + 1]
        length = int.from_bytes(jpeg[offset + 2:offset + 4], 'big')
        if length < 2 or offset + 2 + length > len(jpeg):
            raise ValueError('truncated JPEG header segment')
        segment = jpeg[offset + 4:offset + 2 + length]
        if marker == SOF_LOSSLESS:
            if (len(segment) != 15 or segment[0] != 8 or
                    int.from_bytes(segment[1:3], 'big') != SIDE or
                    int.from_bytes(segment[3:5], 'big') != SIDE or segment[5] != 3 or frame_seen):
                raise ValueError('expected one 256x256 8-bit three-component SOF3 frame')
            frame_seen = True
        if marker == START_OF_SCAN:
            if not frame_seen or len(segment) != 10 or segment[0] != 3 or segment[-3:] != b'\1\0\0':
                raise ValueError('expected lossless predictor 1 and point transform 0')
            return
        offset += 2 + length
    raise ValueError('missing lossless JPEG frame/scan')


class LosslessJpegCodec:
    """Caller supplies an existing trusted libjpeg-turbo 3 library; no installation/build."""

    def __init__(self, library):
        self.library_path = Path(library).resolve(strict=True)
        self._lib = c.CDLL(str(self.library_path))
        self._bind('tj3Init', c.c_void_p, c.c_int)
        self._bind('tj3Set', c.c_int, c.c_void_p, c.c_int, c.c_int)
        self._bind('tj3Destroy', None, c.c_void_p)
        self._bind('tj3GetErrorStr', c.c_char_p, c.c_void_p)
        self._bind('tj3Compress8', c.c_int, c.c_void_p, BYTE_POINTER, c.c_int, c.c_int,
                   c.c_int, c.c_int, c.POINTER(BYTE_POINTER), c.POINTER(c.c_size_t))
        self._bind('tjInitDecompress', c.c_void_p)
        self._bind('tjDestroy', c.c_int, c.c_void_p)
        self._bind('tjDecompressHeader3', c.c_int, c.c_void_p, BYTE_POINTER, c.c_ulong,
                   c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_int))
        self._bind('tjDecompress2', c.c_int, c.c_void_p, BYTE_POINTER, c.c_ulong,
                   BYTE_POINTER, c.c_int, c.c_int, c.c_int, c.c_int, c.c_int)

    def _bind(self, name, result, *arguments):
        function = getattr(self._lib, name)
        function.restype = result
        function.argtypes = list(arguments)

    def _check(self, result, handle):
        if result != 0:
            message = self._lib.tj3GetErrorStr(handle)
            raise ValueError(message.decode('utf-8', errors='replace') if message else 'TurboJPEG error')

    def encode(self, rgb_bottom_up):
        if len(rgb_bottom_up) != PIXEL_BYTES:
            raise ValueError('expected exactly 256x256 RGB pixels')
        source = (c.c_ubyte * PIXEL_BYTES).from_buffer_copy(rgb_bottom_up)
        destination = (c.c_ubyte * JPEG_CAPACITY)()
        pointer = c.cast(destination, BYTE_POINTER)
        size = c.c_size_t(JPEG_CAPACITY)
        handle = self._lib.tj3Init(TJINIT_COMPRESS)
        if not handle:
            raise ValueError('cannot initialize TurboJPEG lossless encoder')
        try:
            parameters = ((TJPARAM_BOTTOMUP, 1), (TJPARAM_NOREALLOC, 1),
                          (TJPARAM_COLORSPACE, TJCS_RGB), (TJPARAM_LOSSLESS, 1),
                          (TJPARAM_LOSSLESSPSV, 1), (TJPARAM_LOSSLESSPT, 0))
            for parameter, value in parameters:
                self._check(self._lib.tj3Set(handle, parameter, value), handle)
            self._check(self._lib.tj3Compress8(handle, source, SIDE, 0, SIDE, TJPF_RGB,
                                              c.byref(pointer), c.byref(size)), handle)
            if size.value > JPEG_CAPACITY or c.addressof(pointer.contents) != c.addressof(destination):
                raise ValueError('encoder exceeded fixed output allocation')
            jpeg = bytes(destination[:size.value])
        finally:
            self._lib.tj3Destroy(handle)
        validate_lossless_headers(jpeg)
        return jpeg[:OZJ_PREFIX_BYTES] + jpeg

    def decode_like_mumain(self, ozj):
        if not OZJ_PREFIX_BYTES < len(ozj) <= JPEG_CAPACITY + OZJ_PREFIX_BYTES:
            raise ValueError('invalid OZJ size')
        jpeg = ozj[OZJ_PREFIX_BYTES:]
        if ozj[:OZJ_PREFIX_BYTES] != jpeg[:OZJ_PREFIX_BYTES] or not jpeg.startswith(b'\xff\xd8'):
            raise ValueError('invalid OZJ prefix or JPEG signature')
        validate_lossless_headers(jpeg)
        source = (c.c_ubyte * len(jpeg)).from_buffer_copy(jpeg)
        handle = self._lib.tjInitDecompress()
        if not handle:
            raise ValueError('cannot initialize legacy TurboJPEG decoder')
        try:
            width, height, subsamp, colorspace = (c.c_int() for _ in range(4))
            self._check(self._lib.tjDecompressHeader3(handle, source, len(jpeg), c.byref(width),
                                                     c.byref(height), c.byref(subsamp), c.byref(colorspace)), handle)
            if width.value != SIDE or height.value != SIDE or colorspace.value != TJCS_RGB:
                raise ValueError('expected a 256x256 RGB JPEG without colorspace conversion')
            destination = (c.c_ubyte * PIXEL_BYTES)()
            self._check(self._lib.tjDecompress2(handle, source, len(jpeg), destination,
                                               SIDE, 0, SIDE, TJPF_RGB, TJFLAG_BOTTOMUP), handle)
            return bytes(destination)
        finally:
            self._lib.tjDestroy(handle)
