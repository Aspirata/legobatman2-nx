import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('switch_setup', ROOT/'tools/setup-switch.py')
setup = importlib.util.module_from_spec(spec)


class SwitchSetupTests(unittest.TestCase):
    def test_verifies_all_archives_before_returning_extract_inputs(self):
        spec.loader.exec_module(setup)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root/'tool.pkg').write_bytes(b'synthetic archive')
            package = {'url': 'https://example.invalid/tool.pkg',
                       'sha256': hashlib.sha256(b'synthetic archive').hexdigest()}
            self.assertEqual(setup.verify_archives([package], root), [root/'tool.pkg'])
            (root/'tool.pkg').write_bytes(b'corrupted')
            with self.assertRaises(ValueError):
                setup.verify_archives([package], root)
            (root/'tool.pkg').unlink()
            with self.assertRaises((OSError, ValueError)):
                setup.verify_archives([package], root)
