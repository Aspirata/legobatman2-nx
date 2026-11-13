#!/usr/bin/env python3
"""Preparation tools. No game data, third-party packages or decryption keys.

XEX2 layout reference: xenia-project/xenia, src/xenia/kernel/util/xex2_info.h.
Only the unencrypted container metadata is inspected; the image is not loaded.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import sys

MAX_HEADER_BYTES = 16 * 1024 * 1024


def inspect_xex(path: Path) -> dict:
    with path.open('rb') as stream:
        size = os.fstat(stream.fileno()).st_size
        fixed = stream.read(24)
        if len(fixed) != 24 or fixed[:4] != b'XEX2':
            raise ValueError('Expected a complete XEX2 header; provide extracted default.xex, not an ISO.')
        _, flags, header_size, _, security_offset, count = struct.unpack('>4s5I', fixed)
        table_end = 24 + count * 8
        if not 24 <= header_size <= min(size, MAX_HEADER_BYTES):
            raise ValueError('Invalid XEX2 header size (inspection limit: 16 MiB).')
        if table_end > header_size:
            raise ValueError('Optional header table exceeds XEX2 header bounds.')
        stream.seek(0)
        header = stream.read(header_size)
        if len(header) != header_size:
            raise ValueError('XEX2 header was truncated during inspection.')

        def require_range(offset, length):
            if offset < table_end or length < 0 or offset + length > header_size:
                raise ValueError('Optional header data exceeds bounds or overlaps the header table.')

        entries = {}
        for index in range(count):
            key, value = struct.unpack_from('>II', header, 24 + index * 8)
            if key in entries:
                raise ValueError(f'Duplicate optional header: 0x{key:08X}.')
            entries[key] = value
            words = key & 0xFF
            if words > 1:
                if words == 0xFF:
                    require_range(value, 4)
                    length = struct.unpack_from('>I', header, value)[0]
                    if length < 4:
                        raise ValueError('Invalid variable optional header size.')
                else:
                    length = words * 4
                require_range(value, length)

        execution = None
        if 0x40006 in entries:
            fields = struct.unpack_from('>4I4BI', header, entries[0x40006])
            execution = dict(zip(
                ('media_id', 'version_raw', 'base_version_raw', 'title_id'),
                (f'{value:08X}' for value in fields[:4])))
            execution.update(platform=fields[4], executable_table=fields[5],
                             disc_number=fields[6], disc_count=fields[7],
                             savegame_id=f'{fields[8]:08X}')
        digest = hashlib.sha256()
        stream.seek(0)
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
        final = os.fstat(stream.fileno())
        if final.st_size != size:
            raise ValueError('Input size changed during inspection; wait for the copy to finish.')
    return {
        'schema_version': 1, 'format': 'XEX2', 'size_bytes': size,
        'sha256': digest.hexdigest(), 'module_flags': f'0x{flags:08X}',
        'header_size': header_size, 'security_offset': security_offset,
        'optional_header_count': count, 'execution_info': execution,
        'entry_point': f'0x{entries[0x10100]:08X}' if 0x10100 in entries else None,
        'image_base': f'0x{entries[0x10201]:08X}' if 0x10201 in entries else None,
        'game_compatibility_verified': False,
        'scope': 'Container metadata only; payload, signatures and game identity are not validated.',
    }


def doctor() -> dict:
    commands = ('git', 'cmake', 'ninja', 'clang', 'clang++', 'rexglue',
                'aarch64-none-elf-g++', 'elf2nro', 'uam', 'dxc', 'spirv-cross',
                'podman', 'docker', 'wsl')
    devkit = os.environ.get('DEVKITPRO')
    components = ('devkitA64/bin', 'libnx/include/switch.h',
                  'portlibs/switch/include/deko3d.h', 'tools/bin')
    return {
        'schema_version': 1, 'host': platform.system(), 'python': platform.python_version(),
        'preparation_tools_ready': sys.version_info >= (3, 11),
        'commands_in_path': {name: shutil.which(name) for name in commands},
        'devkitpro': devkit,
        'devkitpro_components': {part: bool(devkit and (Path(devkit) / part).exists())
                                for part in components},
        'build_verified': False,
        'note': 'Discovery only. Missing tools are null. WSL/container executables do not prove a working Linux environment.',
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('doctor', help='Report tool availability without installing or executing tools.')
    inspect = commands.add_parser('inspect-xex', help='Read XEX2 metadata and SHA-256 without modifying the file.')
    inspect.add_argument('path', type=Path)
    args = parser.parse_args()
    try:
        result = doctor() if args.command == 'doctor' else inspect_xex(args.path)
    except (OSError, ValueError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, ensure_ascii=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
