"""Startup roots must survive the Horizon SDK's POSIX absolute() calls."""
from pathlib import Path, PurePosixPath
import posixpath
import re
import unittest


class SwitchPathsTests(unittest.TestCase):
    def test_startup_paths_do_not_gain_a_second_nro_directory(self):
        source = (Path(__file__).resolve().parents[1] /
                  'src/platform/switch/main.cpp').read_text()
        paths = dict(re.findall(r'paths\.(\w+)\s*=\s*"([^"]+)";', source))
        expected = {
            'game_data_root': 'game', 'user_data_root': 'userdata',
            'cache_root': 'cache', 'metadata_root': 'metadata',
            'config_path': 'legobatman2.toml',
        }
        self.assertEqual(paths.keys(), expected.keys())
        cwd = 'sdmc:/switch/legobatman2'
        for key, suffix in expected.items():
            with self.subTest(path=key):
                path = paths[key]
                # libstdc++ on Horizon uses POSIX rules: "sdmc:" is not a root.
                self.assertTrue(PurePosixPath(path).is_absolute(), path)
                resolved = posixpath.normpath(posixpath.join(cwd, path))
                self.assertEqual(resolved, f'/switch/legobatman2/{suffix}')


if __name__ == '__main__':
    unittest.main()
