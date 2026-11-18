"""Exercise the exact copy-shader format guard used by the Vulkan translator."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
stage = root / 'work/analysis/switch-memexport-format'
stage.mkdir(parents=True, exist_ok=True)
compiler = root / 'work/toolchain/msys64/mingw64/bin/g++.exe'
env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ['PATH'])
exe = stage / 'test.exe'
subprocess.run([str(compiler), '-std=c++20', '-O2', '-ffp-contract=off',
                '-I', str(root / 'src/platform/switch'),
                str(root / 'tests/switch_memexport_format.cpp'), '-o', str(exe)],
               env=env, check=True)
subprocess.run([str(exe)], env=env, check=True)
