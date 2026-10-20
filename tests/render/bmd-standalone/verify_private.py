"""Read-only native loader check against an independent converter manifest.
No game assets or decoded models are written. One subprocess/file bounds memory.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', required=True, type=Path)
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    counts, failures = Counter(), []
    entries = {path: value for path, value in manifest['files'].items()
               if value['status'] != 'ignored'}
    for number, (relative, entry) in enumerate(entries.items(), 1):
        path = args.data / relative
        result = subprocess.run([str(args.probe), str(path)], capture_output=True, timeout=60)
        expected_hash = entry.get('payload_sha256')
        if expected_hash:
            success = result.returncode == 0 and hashlib.sha256(result.stdout).hexdigest() == expected_hash
            counts['payload_match' if success else 'failure'] += 1
        else:
            success = result.returncode == 1 and not result.stdout
            counts['expected_reject' if success else 'failure'] += 1
        if not success:
            failures.append(dict(path=relative, returncode=result.returncode,
                                 expected_hash=expected_hash, stderr=result.stderr.decode(errors='replace')))
        if number % 500 == 0:
            print(number, dict(counts), flush=True)
    report = dict(counts=counts, models=len(entries), failures=failures)
    args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
