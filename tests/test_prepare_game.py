from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PrepareOutputTests(unittest.TestCase):
    def test_manifest_cannot_replace_input_or_game_file(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            image = root / 'disc.iso'
            image.write_bytes(b'input must remain unchanged')
            for manifest in (image, root / 'game/Default.xex'):
                run = subprocess.run([sys.executable, str(ROOT / 'tools/prepare-game.py'),
                    str(image), '--destination', str(root/'game'), '--manifest', str(manifest)],
                    capture_output=True, text=True)
                self.assertNotEqual(run.returncode, 0)
                self.assertIn('manifest', run.stderr.lower())
                self.assertEqual(image.read_bytes(), b'input must remain unchanged')
