"""Run the production timestamp recorder against a Vulkan recording backend."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
stage = root / 'work/analysis/switch-gpu-timestamps-test'
stage.mkdir(parents=True, exist_ok=True)
compiler = root / 'work/toolchain/msys64/mingw64/bin/g++.exe'
env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ['PATH'])
test = root / 'tests/switch_gpu_timestamps'
exe = stage / 'test.exe'
subprocess.run([str(compiler), '-std=c++23', '-O2', '-I', str(test / 'fake'),
               '-I', str(root / 'src/platform/switch'), '-I',
               str(root / 'third_party/nfsmw-nx/sdk/thirdparty/vulkan-headers/include'),
               str(test / 'main.cpp'), str(root / 'src/platform/switch/gpu_timestamps.cpp'),
               '-o', str(exe)], env=env, check=True)
subprocess.run([str(exe)], env=env, check=True)
