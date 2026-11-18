#!/usr/bin/env python3
"""Stage three original guest functions for a local ARM64 compatibility test."""
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    output = ROOT / 'work/analysis/switch-compat/stage'
    output.mkdir(parents=True, exist_ok=True)
    generated = ROOT / 'generated/default'
    shutil.copy2(generated / 'legobatman2_pch.h', output / 'legobatman2_pch.h')
    chunks = ['#include "legobatman2_pch.h"\nREX_EXTERN(sub_82556994);\n']
    records = {}
    for address, unit in {'82556980': 4, '82556994': 85, '825CB588': 117}.items():
        source = generated / f'legobatman2_recomp.{unit}.cpp'
        # Partition numbers can change after codegen; locate the exact entry.
        marker = f'DEFINE_REX_FUNC(sub_{address}) {{'
        if not source.exists() or marker not in source.read_text():
            matches = [p for p in generated.glob('*_recomp.*.cpp') if marker in p.read_text()]
            if len(matches) != 1:
                raise ValueError(f'Expected exactly one original entry {address}')
            source = matches[0]
        match = re.search(r'^DEFINE_REX_FUNC\(sub_' + address + r'\) \{.*?(?=^DEFINE_REX_FUNC|\Z)',
                          source.read_text(), re.M | re.S)
        if not match:
            raise ValueError(f'Cannot extract original entry {address}')
        chunks.append(match[0])
        records[address] = {'source': source.name,
                            'sha256': hashlib.sha256(match[0].encode()).hexdigest()}
    (output / 'guest.cpp').write_text('\n'.join(chunks))
    (output / 'extraction.json').write_text(json.dumps(records, indent=2))
    print(output)


if __name__ == '__main__':
    main()
