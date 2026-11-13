#!/usr/bin/env python3
"""Import the selected local disc. Never install tools or alter the ISO."""
import argparse
import json
from pathlib import Path
import sys

from disc import extract_game
from project import inspect_xex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--destination', type=Path, default=Path('game'))
    parser.add_argument('--manifest', type=Path, default=Path('work/analysis/import.json'))
    args = parser.parse_args()
    try:
        image = args.image.resolve(strict=True)
        destination = args.destination.resolve()
        for output in (args.manifest, args.manifest.with_suffix('.json.tmp')):
            resolved = output.resolve()
            if (resolved == image or resolved.is_relative_to(destination)
                    or (output.exists() and output.samefile(image))):
                raise ValueError('Manifest must be outside the game root and cannot replace the input image.')
        manifest = extract_game(args.image, args.destination)
        config = json.loads((Path(__file__).resolve().parents[1] / 'config/game-build.json').read_text())
        for key in ('iso_sha256', 'iso_size_bytes'):
            if manifest[key] != config[key]:
                raise ValueError(f'Unsupported disc: {key} mismatch; no manifest published.')
        xex = inspect_xex(args.destination / config['xex_path'])
        if xex['sha256'] != config['xex_sha256']:
            raise ValueError('Unsupported XEX hash; no manifest published.')
        for key in ('title_id', 'media_id'):
            if xex['execution_info'][key] != config[key]:
                raise ValueError(f'XEX {key} mismatch.')
        for key in ('entry_point', 'image_base'):
            if xex[key] != config[key]:
                raise ValueError(f'XEX {key} mismatch.')
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.manifest.with_suffix('.json.tmp')
        temporary.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        temporary.replace(args.manifest)
        print(f'Verified {len(manifest["files"])} files; manifest: {args.manifest}')
        return 0
    except (OSError, ValueError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
