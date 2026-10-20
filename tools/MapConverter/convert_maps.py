"""Audit or convert maps into a fresh offline directory; never modify inputs."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import subprocess
import sys

from map_format import (MAX_FILE_BYTES, decode, downgrade_objects, encode_legacy,
                        inspect_payload, sha256)


def discover(source):
    if source.is_file():
        paths = [source]
    else:
        paths = sorted(path for path in source.rglob('*') if path.is_file() and
                       path.suffix.lower() in ('.att', '.map', '.obj'))
    if not paths:
        raise ValueError('no ATT/MAP/OBJ files found')
    return paths


def read_input(path, root):
    if not path.resolve().is_relative_to(root):
        raise ValueError('input symlink escapes the source root')
    if path.stat().st_size > MAX_FILE_BYTES:
        raise ValueError('file exceeds size limit')
    return path.read_bytes()


def inspect_file(path, data, decoder):
    kind = path.suffix.lower()[1:]
    payload, envelope = decode(data, kind, decoder)
    metadata = inspect_payload(payload, kind)
    metadata.update(source_bytes=len(data), source_sha256=sha256(data),
                    payload_sha256=sha256(payload), envelope=envelope, kind=kind)
    folder = re.fullmatch(r'World(\d+)', path.parent.name, re.IGNORECASE)
    if folder and int(folder[1]) != metadata['world_byte']:
        metadata['supported'] = False
        metadata['unsupported_reason'] = 'world header differs from World directory; explicit map mapping required'
    return payload, metadata


def write_converted(destination, payload, metadata):
    kind = metadata['kind']
    sidecar = None
    if kind == 'obj':
        payload, sidecar = downgrade_objects(payload)
    encoded = encode_legacy(payload, kind)
    decoded, envelope = decode(encoded, kind, None)
    if decoded != payload or envelope != 'legacy':
        raise ValueError('legacy output roundtrip mismatch')
    inspect_payload(decoded, kind)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('xb') as output:
        output.write(encoded)
    if destination.read_bytes() != encoded:
        raise ValueError('written output verification failed')
    metadata.update(output_bytes=len(encoded), output_sha256=sha256(encoded),
                    output_payload_sha256=sha256(payload), roundtrip_verified=True)
    if sidecar is not None:
        sidecar.update(source_file_sha256=metadata['source_sha256'], output_file_sha256=sha256(encoded))
        sidecar_path = destination.with_suffix(destination.suffix + '.lighting.json')
        sidecar_bytes = (json.dumps(sidecar, indent=2) + '\n').encode('utf-8')
        with sidecar_path.open('xb') as output:
            output.write(sidecar_bytes)
        metadata.update(sidecar_name=sidecar_path.name, sidecar_sha256=sha256(sidecar_bytes),
                        source_payload_reconstruction_verified=True)


def process(path, root, args):
    entry = dict(path=path.relative_to(root).as_posix(), status='error')
    try:
        data = read_input(path, root)
        entry.update(source_bytes=len(data), source_sha256=sha256(data))
        payload, metadata = inspect_file(path, data, args.decoder)
        entry.update(metadata)
        if not metadata['supported']:
            entry['status'] = 'unsupported'
            return entry
        if metadata['runtime_losses'] and not args.allow_lighting_loss:
            entry.update(status='withheld', reason='loss requires explicit --allow-lighting-loss')
            return entry
        entry['status'] = 'ready-limited' if metadata['runtime_losses'] else 'ready'
        if args.convert:
            write_converted(args.output / entry['path'], payload, entry)
            entry['status'] = 'converted-limited' if metadata['runtime_losses'] else 'converted'
        return entry
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        entry.update(status='error', reason=str(error))
        return entry


def provenance(decoder):
    base = Path(__file__).resolve().parent
    revision = subprocess.run(['git', '-C', str(base), 'rev-parse', 'HEAD'],
                              capture_output=True, text=True, check=True).stdout.strip()
    result = dict(mumain_base_revision=revision,
                  python=sys.version, scripts={name: sha256((base / name).read_bytes())
                                              for name in ('convert_maps.py', 'map_format.py')})
    if decoder:
        result.update(decoder_path=str(decoder), decoder_sha256=sha256(decoder.read_bytes()))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='fresh directory; existing paths are refused')
    parser.add_argument('--decoder', type=Path, help='locally built map_envelope_decode executable')
    parser.add_argument('--convert', action='store_true', help='write compatible files in addition to manifest')
    parser.add_argument('--allow-lighting-loss', action='store_true',
                        help='allow OBJ v1 lighting loss (saved in sidecar); does not permit ATT attribute loss')
    args = parser.parse_args()
    try:
        source = args.input.resolve(strict=True)
        root = source.parent if source.is_file() else source
        args.output = args.output.resolve()
        if args.output.is_relative_to(root) or root.is_relative_to(args.output):
            raise ValueError('input and output directory trees must be disjoint')
        if args.output.exists():
            raise ValueError('output already exists; choose a fresh offline directory')
        if args.decoder:
            args.decoder = args.decoder.resolve(strict=True)
        paths = discover(source)
        identity = provenance(args.decoder)
        args.output.mkdir(parents=True, exist_ok=False)
        entries = []
        for path in paths:
            entry = process(path, root, args)
            entries.append(entry)
        manifest = dict(schema_version=1, mode='convert' if args.convert else 'audit',
                        source_root=str(root), output_root=str(args.output), provenance=identity,
                        allow_lighting_loss=args.allow_lighting_loss,
                        integrity='no CRC/MAC in format; structural validation and SHA256 are not source authentication',
                        runtime_tested=False, summary=dict(Counter(e['status'] for e in entries)), files=entries)
        (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(manifest['summary'], sort_keys=True))
        print(args.output / 'manifest.json')
        return 1 if any(e['status'] in ('error', 'unsupported', 'withheld') for e in entries) else 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(2, f'{error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
