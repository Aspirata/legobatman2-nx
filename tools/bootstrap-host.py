#!/usr/bin/env python3
"""Fetch pinned Microsoft development archives locally. No installer or registry edits.

Microsoft's package licenses apply; these files are local development dependencies,
not distributable game assets. CMake/Ninja are installed separately into work/.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT/'work/toolchain'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def fetch(url, name, expected):
    target = TOOLS/name
    if not target.exists():
        temporary = target.with_suffix(target.suffix+'.part')
        with urllib.request.urlopen(url, timeout=120) as source, temporary.open('wb') as out:
            shutil.copyfileobj(source, out, 1024*1024)
        if sha(temporary) != expected:
            temporary.unlink()
            raise ValueError(f'Archive checksum mismatch: {name}')
        temporary.replace(target)
    if sha(target) != expected:
        raise ValueError(f'Existing archive checksum mismatch: {name}')
    return target


def extract(archive, destination, prefix):
    destination = destination.resolve()
    with zipfile.ZipFile(archive) as source:
        for info in source.infolist():
            if not info.filename.startswith(prefix) or info.is_dir():
                continue
            parts = PurePosixPath(info.filename).parts
            if ('..' in parts or '\\' in info.filename or PurePosixPath(info.filename).is_absolute()
                    or any(':' in part for part in parts) or (info.external_attr >> 16) & 0o170000 == 0o120000):
                raise ValueError('Unsafe package path.')
            target = destination.joinpath(*parts)
            if not target.resolve().is_relative_to(destination):
                raise ValueError('Package path escapes destination.')
            expected = hashlib.sha256()
            with source.open(info) as content:
                while chunk := content.read(1024*1024):
                    expected.update(chunk)
            if target.exists():
                if sha(target) != expected.hexdigest():
                    raise ValueError(f'Existing package file differs: {target.name}')
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.open(info) as content, target.open('xb') as out:
                shutil.copyfileobj(content, out, 1024*1024)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    try:
        TOOLS.mkdir(parents=True, exist_ok=True)
        packages = json.loads((ROOT/'config/host-msvc-packages.json').read_text(encoding='utf-8'))
        for package in packages:
            for payload in package['payloads']:
                archive = fetch(payload['url'], payload['fileName'], payload['sha256'])
                extract(archive, TOOLS/'msvc', 'Contents/VC/')
            print('Verified:', package['id'])
        sdk_version = '10.0.26100.9457'
        for package, name, expected in (
            ('microsoft.windows.sdk.cpp', 'windows-sdk-headers.nupkg', '1c8b332abba402808880ddba4d70e663f834ded44cf4771a2c191e9485e7cdcd'),
            ('microsoft.windows.sdk.cpp.x64', 'windows-sdk-x64.nupkg', '52bd17c63d3b6d268f385c358f34d0eb595b9c1deb3273855abd2ec5c095d132')):
            archive = fetch(f'https://api.nuget.org/v3-flatcontainer/{package}/{sdk_version}/{package}.{sdk_version}.nupkg', name, expected)
            extract(archive, TOOLS/'winsdk', 'c/')
            print('Verified:', package)
        return 0
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
