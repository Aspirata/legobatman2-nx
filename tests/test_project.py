import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / 'tools' / 'project.py'


def fixture():
    # Synthetic header only: no game code, key material or copyrighted assets.
    data = bytearray(128)
    struct.pack_into('>4s5I', data, 0, b'XEX2', 1, 128, 0, 0, 3)
    struct.pack_into('>6I', data, 24, 0x40006, 64, 0x10100, 0x82001234,
                     0x10201, 0x82000000)
    struct.pack_into('>4I4BI', data, 64, 0x12345678, 0x10000100, 0x10000000,
                     0x11223344, 2, 0, 1, 1, 0)
    return data + b'synthetic-payload'


class InspectTests(unittest.TestCase):
    def run_tool(self, data, *args):
        with tempfile.TemporaryDirectory(prefix='lb2 test ') as directory:
            path = Path(directory) / 'игра default.xex'
            path.write_bytes(data)
            result = subprocess.run([sys.executable, str(TOOL), 'inspect-xex', str(path), *args],
                                    capture_output=True, text=True, encoding='utf-8')
            self.assertEqual(path.read_bytes(), data)
            if result.returncode:
                self.assertTrue(result.stderr.startswith('error: '), result.stderr)
            return result

    def test_reads_identity_and_hash_without_modifying_input(self):
        data = fixture()
        result = self.run_tool(data)
        self.assertEqual(result.returncode, 0, result.stderr)
        info = json.loads(result.stdout)
        self.assertEqual(info['sha256'], hashlib.sha256(data).hexdigest())
        self.assertEqual(info['execution_info']['title_id'], '11223344')
        self.assertEqual(info['execution_info']['media_id'], '12345678')
        self.assertEqual(info['entry_point'], '0x82001234')
        self.assertEqual(info['image_base'], '0x82000000')
        self.assertFalse(info['game_compatibility_verified'])

    def test_rejects_non_xex(self):
        result = self.run_tool(b'not-an-xex' * 20)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('XEX2', result.stderr)

    def test_rejects_truncated_header(self):
        result = self.run_tool(b'XEX2')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('Traceback', result.stderr)

    def test_rejects_table_outside_header(self):
        data = fixture()
        struct.pack_into('>I', data, 20, 0xFFFFFFFF)
        self.assertNotEqual(self.run_tool(data).returncode, 0)

    def test_rejects_execution_info_in_payload(self):
        data = fixture()
        struct.pack_into('>I', data, 28, 120)
        self.assertNotEqual(self.run_tool(data).returncode, 0)

    def test_rejects_execution_info_overlapping_table(self):
        data = fixture()
        struct.pack_into('>I', data, 28, 24)
        self.assertNotEqual(self.run_tool(data).returncode, 0)

    def test_rejects_duplicate_headers(self):
        data = fixture()
        struct.pack_into('>I', data, 32, 0x40006)
        self.assertNotEqual(self.run_tool(data).returncode, 0)

    def test_missing_identity_is_unknown(self):
        data = fixture()
        struct.pack_into('>I', data, 24, 0x20200)
        result = self.run_tool(data)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNone(json.loads(result.stdout)['execution_info'])

    def test_variable_header_at_boundary(self):
        data = fixture()
        struct.pack_into('>II', data, 24, 0x183FF, 120)
        struct.pack_into('>I', data, 120, 8)
        result = self.run_tool(data)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_variable_header_outside_bounds(self):
        for offset, length in ((120, 9), (126, 8), (120, 0)):
            with self.subTest(offset=offset, length=length):
                data = fixture()
                struct.pack_into('>II', data, 24, 0x183FF, offset)
                struct.pack_into('>I', data, 120, length)
                self.assertEqual(self.run_tool(data).returncode, 1)

    def test_header_size_exceeds_file(self):
        data = fixture()
        struct.pack_into('>I', data, 8, 0xFFFFFFFF)
        self.assertEqual(self.run_tool(data).returncode, 1)

    def test_doctor_reports_missing_environment_without_claiming_build(self):
        env = dict(os.environ, PATH='', DEVKITPRO='')
        result = subprocess.run([sys.executable, str(TOOL), 'doctor'], env=env,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertIsNone(report['commands_in_path']['rexglue'])
        self.assertFalse(any(report['devkitpro_components'].values()))
        self.assertFalse(report['build_verified'])

    def test_missing_file_is_clean_error(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([sys.executable, str(TOOL), 'inspect-xex',
                                     str(Path(directory) / 'absent.xex')],
                                    capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('Traceback', result.stderr)


if __name__ == '__main__':
    unittest.main()
