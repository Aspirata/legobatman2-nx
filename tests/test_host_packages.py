import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('bootstrap_host', ROOT/'tools/bootstrap-host.py')
bootstrap = importlib.util.module_from_spec(spec)


class HostArchiveTests(unittest.TestCase):
    def test_detects_changed_or_incomplete_compiler_headers(self):
        spec.loader.exec_module(bootstrap)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            header = root/'example.h'
            header.write_bytes(b'original')
            expected = bootstrap.tree_digest(root)
            bootstrap.verify_tree(root, expected)
            header.write_bytes(b'changed')
            with self.assertRaises(ValueError):
                bootstrap.verify_tree(root, expected)
            header.unlink()
            with self.assertRaises(ValueError):
                bootstrap.verify_tree(root, expected)

    def test_rejects_traversal_inside_selected_prefix(self):
        spec.loader.exec_module(bootstrap)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive = root/'bad.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('Contents/VC/../../../../outside', b'bad')
            with self.assertRaises(ValueError):
                bootstrap.extract(archive, root/'out', 'Contents/VC/')
            self.assertFalse((root/'outside').exists())

    def test_extracts_selected_files_and_keeps_equal_existing(self):
        spec.loader.exec_module(bootstrap)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive = root/'tools.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('Contents/VC/include/example.h', b'synthetic')
                z.writestr('irrelevant', b'not selected')
            bootstrap.extract(archive, root/'out', 'Contents/VC/')
            bootstrap.extract(archive, root/'out', 'Contents/VC/')
            target = root/'out/Contents/VC/include/example.h'
            self.assertEqual(target.read_bytes(), b'synthetic')
            self.assertFalse((root/'out/irrelevant').exists())
            target.write_bytes(b'changed')
            with self.assertRaises(ValueError):
                bootstrap.extract(archive, root/'out', 'Contents/VC/')
