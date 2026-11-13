#!/usr/bin/env python3
"""Verify and unpack pinned Windows Switch tools locally, without an installer.

Obtain the exact archives in config/switch-packages.json from the official URLs.
If command-line requests receive HTTP403, downloading in a browser may work.
No package install scripts, registry updates or system environment changes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]


def verify_archives(packages, directory):
    paths = []
    for package in packages:
        filename = urlsplit(package['url']).path.rsplit('/', 1)[-1]
        if not filename or filename in ('.', '..') or '\\' in filename:
            raise ValueError('Invalid pinned package filename.')
        path = directory/filename
        with path.open('rb') as stream:
            actual = hashlib.file_digest(stream, 'sha256').hexdigest()
        if actual != package['sha256']:
            raise ValueError(f'Package checksum mismatch: {filename}')
        paths.append(path)
    return paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archives', type=Path, default=ROOT/'work/toolchain/switch-packages')
    args = parser.parse_args()
    try:
        if os.name != 'nt':
            raise ValueError('This pinned package set is for a Windows host.')
        packages = json.loads((ROOT/'config/switch-packages.json').read_text())['packages']
        archives = verify_archives(packages, args.archives.resolve())
        destination = ROOT/'work/toolchain/switch-root'
        destination.mkdir(parents=True, exist_ok=True)
        if shutil.disk_usage(destination).free < 512*1024*1024:
            raise ValueError('Need at least 512 MiB free for this toolchain extraction.')
        tar = Path(os.environ['SystemRoot'])/'System32/tar.exe'
        for archive in archives:
            # Only the developer prefix: exclude package-manager metadata/scripts.
            subprocess.run([str(tar), '-xf', str(archive), '-C', str(destination), 'opt/devkitpro'], check=True)
            print('Verified and extracted:', archive.name)
        compiler = destination/'opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++.exe'
        subprocess.run([str(compiler), '--version'], check=True)
        print('Portable Switch prefix:', destination/'opt/devkitpro')
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
