"""Prepare a local SD folder; never copies Xbox system updates or PC saves."""
from pathlib import Path
import hashlib
import json
import shutil
import struct


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    root = Path(__file__).resolve().parents[1]
    build = root / 'build/switch-game'
    nro = build / 'legobatman2.nro'
    elf = build / 'legobatman2'
    with elf.open('rb') as stream:
        header = stream.read(64)
    if header[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', header, 18)[0] != 183:
        raise ValueError('Expected a little-endian ARM64 ELF')
    with nro.open('rb') as stream:
        header = stream.read(128)
    if header[16:20] != b'NRO0':
        raise ValueError('Expected a packaged Switch NRO')
    expected = json.loads((root / 'config/game-build.json').read_text())['xex_sha256']
    if digest(root / 'game/Default.xex') != expected:
        raise ValueError('Original XEX differs from the compiled game')
    target = build / 'sdcard/switch/legobatman2'
    (target / 'game').mkdir(parents=True, exist_ok=True)
    sources = [nro] + [root / 'game' / name for name in (
        'Default.xex', 'GAME.DAT', 'GAME0.DAT', 'GAME1.DAT', 'GAME2.DAT', '__DISC__', 'NxeArt')]
    rows = []
    for source in sources:
        dest = target / (source.name if source == nro else f'game/{source.name}')
        sha = digest(source)
        if not dest.is_file() or dest.stat().st_size != source.stat().st_size or digest(dest) != sha:
            shutil.copyfile(source, dest)
        if digest(dest) != sha:
            raise ValueError(f'Copy verification failed: {dest}')
        rows.append({'path': dest.relative_to(build / 'sdcard').as_posix(),
                     'bytes': dest.stat().st_size, 'sha256': sha})
        print(f'Verified: {dest.name} ({dest.stat().st_size:,} bytes)', flush=True)
    manifest = {'schema_version': 1, 'hardware_tested': False, 'files': rows}
    (build / 'sdcard-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (target / 'README.txt').write_text(
        'LEGO Batman 2 for Nintendo Switch\n\n'
        'Launch through hbmenu in application mode (hold R while starting a game).\n'
        'Game data is stored in the game folder; keep your own copy.\n'
        'Saves are in userdata, logs are in logs. Preserve userdata when updating.\n'
        'This build is experimental and currently runs at approximately 3 FPS.\n',
        encoding='utf-8',
    )
    print(f'SD folder ready: {build / "sdcard"}', flush=True)


if __name__ == '__main__':
    main()
