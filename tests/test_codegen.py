import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('lb2_codegen', ROOT / 'tools/codegen.py')
codegen = importlib.util.module_from_spec(spec)


class CodegenGuardTests(unittest.TestCase):
    def test_rejects_wrong_xex_before_invoking_tool(self):
        spec.loader.exec_module(codegen)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'wrong.xex'
            path.write_bytes(b'wrong build')
            with self.assertRaisesRegex(ValueError, 'XEX'):
                codegen.verify_inputs(path, Path(temp) / 'never-executed.exe', ROOT)

    def test_rejects_wrong_sdk_binary(self):
        spec.loader.exec_module(codegen)
        # Fully synthetic input: CI remains game-free.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'config').mkdir()
            (root / 'config/game-build.json').write_text(json.dumps({'xex_sha256':
                hashlib.sha256(b'wrong').hexdigest()}))
            (root / 'toolchain-lock.json').write_text(json.dumps({'host_baseline': {'exe_sha256': '0'*64}}))
            xex, sdk = root / 'input.xex', root / 'sdk.exe'
            xex.write_bytes(b'wrong')
            sdk.write_bytes(b'do not execute')
            with self.assertRaisesRegex(ValueError, 'SDK'):
                codegen.verify_inputs(xex, sdk, root)
