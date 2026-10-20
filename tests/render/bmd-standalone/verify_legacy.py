"""Read-only byte regression over an existing v10/v12 Data tree."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess

KEY = bytes.fromhex('d17352f6d29acb273eaf593137b3e7a2')

def decode(data):
    if data[3] == 10:
        return data[4:]
    assert int.from_bytes(data[4:8], 'little') == len(data)-8
    result, state = bytearray(), 0x5e
    for index, value in enumerate(data[8:]):
        result.append(((value ^ KEY[index % 16]) - state) & 255)
        state = (value + 0x3d) & 255
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--expected-reject', action='append', default=[],
                        help='exact relative path of a separately diagnosed unsafe model')
    args = parser.parse_args()
    expected_rejects = set(args.expected_reject)
    counts, failures = Counter(), []
    for path in args.data.rglob('*'):
        if path.suffix.lower() != '.bmd': continue
        with path.open('rb') as source: header=source.read(4)
        if header not in (b'BMD\x0a', b'BMD\x0c'): continue
        data=path.read_bytes()
        result=subprocess.run([str(args.probe),str(path)],capture_output=True,timeout=60)
        expected=decode(data)
        success=result.returncode==0 and hashlib.sha256(result.stdout).digest()==hashlib.sha256(expected).digest()
        relative = path.relative_to(args.data).as_posix()
        if relative in expected_rejects:
            success = result.returncode == 1 and not result.stdout
            counts['expected_reject' if success else 'failure'] += 1
            expected_rejects.remove(relative)
        else:
            counts['v'+str(header[3])+'_match' if success else 'failure'] += 1
        if not success: failures.append(dict(path=str(path.relative_to(args.data)),returncode=result.returncode))
        if sum(counts.values())%500==0: print(dict(counts),flush=True)
    for missing in expected_rejects:
        failures.append(dict(path=missing, reason='expected rejection was not exercised'))
    report=dict(counts=counts,failures=failures)
    args.report.write_text(json.dumps(report,indent=2))
    print(json.dumps(report))
    return bool(failures)

if __name__=='__main__': raise SystemExit(main())
