"""Synthetic XDVDFS only; no game bytes."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import disc

BASE = 0x2080000
SECTOR = 2048


def node(name, sector, size, attr=0, left=0, right=0):
    raw = name.encode('ascii')
    return struct.pack('<HHIIBB', left, right, sector, size, attr, len(raw)) + raw


def image(path, table=None):
    # Sparse image also exercises offsets larger than the tiny fixture itself.
    with path.open('wb') as f:
        f.truncate(BASE + 45 * SECTOR)
        volume = bytearray(SECTOR)
        volume[:20] = volume[-20:] = b'MICROSOFT*XBOX*MEDIA'
        struct.pack_into('<II', volume, 20, 33, SECTOR)
        f.seek(BASE + 32 * SECTOR)
        f.write(volume)
        directory = bytearray(SECTOR)
        directory[:len(table or b'')] = table or b''
        if table is None:
            for off, entry in ((0, node('Dir', 34, SECTOR, 16, 8, 16)),
                               (32, node('A', 35, 3)), (64, node('Zero', 0, 0))):
                directory[off:off+len(entry)] = entry
        f.seek(BASE + 33 * SECTOR)
        f.write(directory)
        f.seek(BASE + 34 * SECTOR)
        f.write(node('Child', 36, 4))
        f.seek(BASE + 35 * SECTOR)
        f.write(b'abc')
        f.seek(BASE + 36 * SECTOR)
        f.write(b'data')


class DiscTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.iso = self.root / 'disc.iso'
        image(self.iso)

    def test_partition_tree_and_empty_file(self):
        entries = {e.relative_path: e for e in disc.read_directory(self.iso)}
        self.assertEqual(set(entries), {'A', 'Dir', 'Dir/Child', 'Zero'})
        self.assertTrue(entries['Dir'].is_directory)
        self.assertEqual(entries['Dir/Child'].relative_sector, 36)
        self.assertEqual(entries['Zero'].size, 0)

    def test_streaming_extract_and_idempotent_verification(self):
        dest = self.root / 'game'
        manifest = disc.extract_game(self.iso, dest)
        self.assertEqual((dest / 'Dir/Child').read_bytes(), b'data')
        self.assertEqual((dest / 'Zero').read_bytes(), b'')
        files = {e['path']: e for e in manifest['files']}
        self.assertEqual(files['A']['sha256'], hashlib.sha256(b'abc').hexdigest())
        self.assertEqual(disc.extract_game(self.iso, dest), manifest)
        (dest / 'A').write_bytes(b'bad')
        with self.assertRaises(ValueError):
            disc.extract_game(self.iso, dest)
        self.assertEqual((dest / 'A').read_bytes(), b'bad')

    def test_bad_descriptors_and_truncation(self):
        for offset in (BASE + 32*SECTOR, BASE + 33*SECTOR - 20):
            image(self.iso)
            with self.iso.open('r+b') as f:
                f.seek(offset)
                f.write(b'X')
            with self.assertRaises(ValueError):
                disc.read_directory(self.iso)
        self.iso.write_bytes(b'truncated')
        with self.assertRaises(ValueError):
            disc.read_directory(self.iso)

    def test_invalid_nodes(self):
        for entry in (node('../evil', 35, 3), node('..', 35, 3),
                      node('C:evil', 35, 3), node('A.', 35, 3),
                      node('CON', 35, 3), node('A', 500, 3),
                      node('A', 35, 3, right=600),
                      node('Dir', 33, SECTOR, 16)):
            with self.subTest(entry=entry):
                image(self.iso, entry)
                with self.assertRaises(ValueError):
                    disc.read_directory(self.iso)

    def test_cycle_and_case_collision(self):
        for second in (node('B', 35, 3, right=8), node('a', 35, 3)):
            table = bytearray(64)
            first = node('A', 35, 3, right=8)
            table[:len(first)] = first
            table[32:32+len(second)] = second
            image(self.iso, table)
            with self.assertRaises(ValueError):
                disc.read_directory(self.iso)

    def test_destination_cannot_be_input_or_existing_file(self):
        for dest in (self.iso, self.root / 'occupied'):
            if dest != self.iso:
                dest.write_bytes(b'keep')
            with self.assertRaises((ValueError, OSError)):
                disc.extract_game(self.iso, dest)


if __name__ == '__main__':
    unittest.main()
