"""Bounded ATT/MAP and OBJ parser for the unchanged MuMain legacy readers."""
from collections import Counter
import hashlib
import math
import struct
import subprocess

CELL_COUNT = 256 * 256
MAP_BYTES = 2 + CELL_COUNT * 3
ATT_BYTES = (4 + CELL_COUNT, 4 + CELL_COUNT * 2)
OBJECT_HEADER = struct.Struct('<BBh')
OBJECT_RECORD = struct.Struct('<h7f')
OBJECT_STRIDES = {0: 30, 1: 32, 2: 33, 3: 45}
MAX_FILE_BYTES = 2 * 1024 * 1024
SIGNATURE_BYTES = 4
ENVELOPE_BYTES = 34
ENVELOPE_OVERHEAD = SIGNATURE_BYTES + ENVELOPE_BYTES
DECODER_TIMEOUT_SECONDS = 30
LOW_ATTRIBUTE_ERROR_THRESHOLD = 128
MAP_KEY = bytes.fromhex('d17352f6d29acb273eaf593137b3e7a2')
BUX_KEY = bytes.fromhex('fccfab')


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def legacy_decrypt(data):
    output = bytearray()
    seed = 0x5E
    for index, value in enumerate(data):
        output.append(((value ^ MAP_KEY[index % len(MAP_KEY)]) - seed) & 255)
        seed = (value + 0x3D) & 255
    return bytes(output)


def legacy_encrypt(data):
    output = bytearray()
    seed = 0x5E
    for index, value in enumerate(data):
        encrypted = ((value + seed) & 255) ^ MAP_KEY[index % len(MAP_KEY)]
        output.append(encrypted)
        seed = (encrypted + 0x3D) & 255
    return bytes(output)


def bux(data):
    return bytes(value ^ BUX_KEY[index % len(BUX_KEY)] for index, value in enumerate(data))


def decode(data, kind, decoder):
    if not data or len(data) > MAX_FILE_BYTES:
        raise ValueError('file is empty or exceeds size limit')
    signature = kind.upper().encode('ascii')
    if kind in ('att', 'map') and data.startswith(signature):
        if data[:4] != signature + b'\1':
            raise ValueError('unknown envelope version')
        expected_sizes = ATT_BYTES if kind == 'att' else (MAP_BYTES,)
        if len(data) - ENVELOPE_OVERHEAD not in expected_sizes:
            raise ValueError('envelope length does not match a supported map payload')
        if decoder is None:
            raise ValueError('ATT/MAP v1 requires --decoder built from source')
        result = subprocess.run([str(decoder)], input=data, capture_output=True, timeout=DECODER_TIMEOUT_SECONDS)
        if result.returncode:
            raise ValueError('envelope decoder failed: ' + result.stderr.decode('utf-8', errors='replace').strip())
        payload = result.stdout
        if len(payload) != len(data) - ENVELOPE_OVERHEAD:
            raise ValueError('envelope decoder returned an unexpected payload length')
        envelope = 'modulus-v1'
    else:
        payload = legacy_decrypt(data)
        envelope = 'legacy'
    return (bux(payload) if kind == 'att' else payload), envelope


def inspect_att(payload):
    if len(payload) not in ATT_BYTES or payload[0] != 0 or payload[2:4] != b'\xff\xff':
        raise ValueError('ATT requires version 0, dimensions 255/255 and exactly 65536 cells')
    width = (len(payload) - 4) // CELL_COUNT
    values = payload[4:] if width == 1 else struct.unpack_from('<65536H', payload, 4)
    high_count = sum(value > 255 for value in values)
    invalid_low_count = sum((value & 255) >= LOW_ATTRIBUTE_ERROR_THRESHOLD for value in values)
    result = dict(world_byte=payload[1], cell_bytes=width,
                  values=dict(sorted(Counter(values).items())), high_flag_cells=high_count,
                  rejected_low_flag_cells=invalid_low_count, supported=not (invalid_low_count or high_count),
                  runtime_losses=[])
    if high_count:
        result['runtime_losses'].append('MuMain masks ATT words with 0xFF; high attribute flags have no runtime effect')
        result['unsupported_reason'] = 'high ATT attribute flags would be lost by the current MuMain loader'
    if invalid_low_count:
        result['unsupported_reason'] = 'MuMain exits when any low attribute byte is >= 128'
    return result


def inspect_map(payload):
    if len(payload) != MAP_BYTES or payload[0] != 0:
        raise ValueError('MAP requires version 0 and exactly three 65536-byte layers')
    return dict(world_byte=payload[1], supported=True, runtime_losses=[],
                layer_sha256=[sha256(payload[2 + layer * CELL_COUNT:2 + (layer + 1) * CELL_COUNT])
                              for layer in range(3)])


def inspect_objects(payload):
    if len(payload) < OBJECT_HEADER.size:
        raise ValueError('truncated OBJ header')
    version, world, count = OBJECT_HEADER.unpack_from(payload)
    if version not in OBJECT_STRIDES or count < 0:
        raise ValueError('unknown OBJ version or negative signed object count')
    stride = OBJECT_STRIDES[version]
    if len(payload) != OBJECT_HEADER.size + count * stride:
        raise ValueError('OBJ count/stride does not consume the whole file')
    flags, types = Counter(), Counter()
    for index in range(count):
        offset = OBJECT_HEADER.size + index * stride
        record = OBJECT_RECORD.unpack_from(payload, offset)
        if record[0] < 0 or not all(math.isfinite(value) for value in record[1:]):
            raise ValueError(f'OBJ record {index} has a negative type or non-finite transform')
        types[record[0]] += 1
        if version >= 1:
            pair = payload[offset + OBJECT_RECORD.size:offset + OBJECT_RECORD.size + 2]
            flags[pair.hex()] += 1
    result = dict(version=version, world_byte=world, count=count, stride=stride,
                  object_types=dict(sorted(types.items())), light_flag_pairs=dict(sorted(flags.items())),
                  supported=version in (0, 1), runtime_losses=[])
    if version >= 1:
        result['runtime_losses'].append('legacy OBJ has no LightEnable1/LightEnable2; appearance is unverified')
    if version >= 2:
        result['unsupported_reason'] = 'OBJ v2/v3 contain additional lighting data outside this converter scope'
    return result


def inspect_payload(payload, kind):
    return {'att': inspect_att, 'map': inspect_map, 'obj': inspect_objects}[kind](payload)


def downgrade_objects(payload):
    metadata = inspect_objects(payload)
    if metadata['version'] == 0:
        return payload, None
    if metadata['version'] != 1:
        raise ValueError('only OBJ v1 can be downgraded')
    output = bytearray(payload[:OBJECT_HEADER.size])
    output[0] = 0
    flags = bytearray()
    for index in range(metadata['count']):
        offset = OBJECT_HEADER.size + index * OBJECT_STRIDES[1]
        output.extend(payload[offset:offset + OBJECT_RECORD.size])
        flags.extend(payload[offset + OBJECT_RECORD.size:offset + OBJECT_STRIDES[1]])
    sidecar = dict(schema_version=1, source_version=1, count=metadata['count'],
                   source_payload_sha256=sha256(payload), target_payload_sha256=sha256(output),
                   light_flags_hex=flags.hex())
    if restore_objects(bytes(output), sidecar) != payload:
        raise ValueError('OBJ sidecar reconstruction failed')
    return bytes(output), sidecar


def restore_objects(payload, sidecar):
    metadata = inspect_objects(payload)
    if (sidecar['schema_version'] != 1 or sidecar['source_version'] != 1 or
            metadata['version'] != 0 or metadata['count'] != sidecar['count'] or
            sha256(payload) != sidecar['target_payload_sha256']):
        raise ValueError('sidecar does not match legacy OBJ')
    flags = bytes.fromhex(sidecar['light_flags_hex'])
    if len(flags) != metadata['count'] * 2:
        raise ValueError('sidecar flag count does not match OBJ')
    output = bytearray(payload[:OBJECT_HEADER.size])
    output[0] = 1
    for index in range(metadata['count']):
        offset = OBJECT_HEADER.size + index * OBJECT_RECORD.size
        output.extend(payload[offset:offset + OBJECT_RECORD.size])
        output.extend(flags[index * 2:index * 2 + 2])
    if sha256(output) != sidecar['source_payload_sha256']:
        raise ValueError('sidecar reconstructed payload hash mismatch')
    return bytes(output)


def encode_legacy(payload, kind):
    return legacy_encrypt(bux(payload) if kind == 'att' else payload)
