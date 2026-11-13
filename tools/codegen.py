#!/usr/bin/env python3
"""Run the pinned host code generator after checking game and SDK identity."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_inputs(xex, sdk, root=ROOT):
    game = json.loads((root / 'config/game-build.json').read_text(encoding='utf-8'))
    lock = json.loads((root / 'toolchain-lock.json').read_text(encoding='utf-8'))
    if digest(xex) != game['xex_sha256']:
        raise ValueError('XEX hash mismatch: import the selected disc first.')
    if digest(sdk) != lock['host_baseline']['exe_sha256']:
        raise ValueError('SDK hash mismatch: this manifest requires the pinned host generator.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=ROOT / 'work/toolchain/rexglue-0.10.0/win-amd64/bin/rexglue.exe')
    parser.add_argument('--regenerate', action='store_true')
    args = parser.parse_args()
    try:
        sdk = args.sdk.resolve(strict=True)
        verify_inputs(ROOT / 'game/Default.xex', sdk)
        log = ROOT / 'work/analysis/codegen.log'
        log.parent.mkdir(parents=True, exist_ok=True)
        command = [str(sdk), 'codegen', 'port/legobatman2_manifest.toml']
        if args.regenerate:
            command.append('--ignore-stamp')
        with log.open('w', encoding='utf-8') as output:
            result = subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT)
        print('\n'.join(log.read_text(encoding='utf-8', errors='replace').splitlines()[-12:]))
        print(f'Codegen exit: {result.returncode}; full log: {log}')
        return result.returncode
    except (OSError, ValueError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
