"""Bounded, read-only XDVDFS reader and streaming local game importer.

No decryption or game-specific patches. Sector addresses are partition-relative.
"""
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import shutil
import struct
import tempfile

SECTOR = 2048
MAGIC = b'MICROSOFT*XBOX*MEDIA'
PARTITIONS = (0, 0x2080000, 0xFD90000, 0x18300000)
VERSION = 1
MAX_DIRECTORY = 16 * 1024 * 1024
MAX_ENTRIES = 100000


@dataclass(frozen=True)
class DiscEntry:
    relative_path: str
    relative_sector: int
    size: int
    is_directory: bool


def _stamp(stream):
    stat = os.fstat(stream.fileno())
    return stat.st_size, stat.st_mtime_ns


def _read(stream, offset, length, size):
    if offset < 0 or length < 0 or offset + length > size:
        raise ValueError('Disc extent outside image.')
    stream.seek(offset)
    data = stream.read(length)
    if len(data) != length:
        raise ValueError('Short read from image.')
    return data


def _name(raw):
    name = raw.decode('ascii')
    reserved = {'CON', 'PRN', 'AUX', 'NUL',
                *(f'COM{i}' for i in range(1, 10)),
                *(f'LPT{i}' for i in range(1, 10))}
    if (not name or name in ('.', '..') or name[-1] in '. '
            or any(ord(c) < 32 or c in '/\\:<>"|?*' for c in name)
            or name.split('.')[0].upper() in reserved):
        raise ValueError('Unsafe or nonportable disc filename.')
    return name


def _directory(stream):
    size = _stamp(stream)[0]
    bases = []
    for base in PARTITIONS:
        offset = base + 32 * SECTOR
        if offset + SECTOR <= size:
            volume = _read(stream, offset, SECTOR, size)
            if volume[:20] == MAGIC and volume[-20:] == MAGIC:
                bases.append((base, volume))
    if len(bases) != 1:
        raise ValueError('Expected one complete XDVDFS volume descriptor.')
    base, volume = bases[0]
    root_sector, root_size = struct.unpack_from('<II', volume, 20)
    pending = [(root_sector, root_size, '', 0)]
    directories, names, entries = set(), set(), []
    while pending:
        sector, length, parent, depth = pending.pop()
        if sector in directories or length > MAX_DIRECTORY or depth > 128:
            raise ValueError('Directory cycle or directory limit exceeded.')
        directories.add(sector)
        table = _read(stream, base + sector * SECTOR, length, size)
        nodes = [0] if length else []
        seen, spans = set(), []
        while nodes:
            off = nodes.pop()
            if off in seen or off + 14 > length:
                raise ValueError('Directory node cycle or truncated node.')
            seen.add(off)
            left, right, extent, count, attr, nl = struct.unpack_from('<HHIIBB', table, off)
            end = off + 14 + nl
            if end > length or any(off < b and end > a for a, b in spans):
                raise ValueError('Truncated or overlapping directory node.')
            spans.append((off, end))
            name = _name(table[off+14:end])
            path = f'{parent}/{name}' if parent else name
            if path.casefold() in names:
                raise ValueError('Duplicate or case-colliding disc path.')
            names.add(path.casefold())
            if base + extent * SECTOR + count > size:
                raise ValueError('Disc extent outside image.')
            directory = bool(attr & 16)
            entries.append(DiscEntry(path, extent, count, directory))
            if len(entries) > MAX_ENTRIES:
                raise ValueError('Disc entry limit exceeded.')
            if directory:
                pending.append((extent, count, path, depth + 1))
            if right:
                nodes.append(right * 4)
            if left:
                nodes.append(left * 4)
    return base, sorted(entries, key=lambda e: e.relative_path)


def read_directory(image: Path) -> list[DiscEntry]:
    with image.open('rb') as stream:
        before = _stamp(stream)
        _, entries = _directory(stream)
        if before != _stamp(stream):
            raise ValueError('Image changed during inventory.')
        return entries


def _hash(stream):
    digest = hashlib.sha256()
    while chunk := stream.read(1024 * 1024):
        digest.update(chunk)
    return digest.hexdigest()


def _target(root, relative):
    target = root.joinpath(*relative.split('/'))
    # Reject links/junctions in every existing path component before writes.
    part = target
    while part != root.parent:
        if part.is_symlink() or (hasattr(part, 'is_junction') and part.is_junction()):
            raise ValueError('Destination contains a link or junction.')
        if part == root:
            break
        part = part.parent
    if not target.resolve().is_relative_to(root):
        raise ValueError('Destination escapes game root.')
    return target


def extract_game(image: Path, destination: Path) -> dict:
    image = image.resolve(strict=True)
    if destination.is_symlink() or (hasattr(destination, 'is_junction') and destination.is_junction()):
        raise ValueError('Game root must not be a link.')
    destination = destination.resolve()
    if destination == image:
        raise ValueError('Destination is input image.')
    with image.open('rb') as stream:
        before = _stamp(stream)
        base, entries = _directory(stream)
        destination.mkdir(parents=True, exist_ok=True)
        required = sum(e.size for e in entries if not e.is_directory
                       and not _target(destination, e.relative_path).exists())
        if shutil.disk_usage(destination).free < required + 64 * 1024 * 1024:
            raise ValueError('Insufficient free space for extracted game and margin.')
        stream.seek(0)
        iso_hash = _hash(stream)
        files = []
        for entry in entries:
            target = _target(destination, entry.relative_path)
            if entry.is_directory:
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists() and (not target.is_file() or target.stat().st_size != entry.size):
                raise ValueError(f'Existing file differs: {entry.relative_path}')
            temporary = None
            try:
                with tempfile.NamedTemporaryFile(dir=target.parent, prefix='.import-', delete=False) as out:
                    temporary = Path(out.name)
                    stream.seek(base + entry.relative_sector * SECTOR)
                    remaining, digest = entry.size, hashlib.sha256()
                    while remaining:
                        chunk = stream.read(min(1024 * 1024, remaining))
                        if not chunk:
                            raise ValueError('Short read while extracting game.')
                        remaining -= len(chunk)
                        digest.update(chunk)
                        out.write(chunk)
                sha = digest.hexdigest()
                if target.exists():
                    with target.open('rb') as existing:
                        if _hash(existing) != sha:
                            raise ValueError(f'Existing file differs: {entry.relative_path}')
                else:
                    if _stamp(stream) != before:
                        raise ValueError('Image changed during extraction.')
                    # No overwrite: publication is atomic on the destination volume.
                    os.link(temporary, target)
                files.append({'path': entry.relative_path, 'size_bytes': entry.size,
                              'sha256': sha, 'runtime': not entry.relative_path.startswith('$SystemUpdate/')})
            finally:
                if temporary is not None:
                    temporary.unlink(missing_ok=True)
        if _stamp(stream) != before:
            raise ValueError('Image changed during extraction; manifest not published.')
    return {'schema_version': 1, 'importer_version': VERSION,
            'iso_size_bytes': before[0], 'iso_sha256': iso_hash,
            'partition_base': base, 'files': files}
