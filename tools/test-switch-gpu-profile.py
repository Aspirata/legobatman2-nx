"""Run the production CPU profiler with deterministic time and a log recorder."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
stage = root / 'work/analysis/switch-gpu-profile-test'
(stage / 'rex').mkdir(parents=True, exist_ok=True)
(stage / 'rex/logging.h').write_text(
    '#pragma once\n#define REXLOG_INFO(...) Capture(__VA_ARGS__)\n'
    '#define REXLOG_WARN(...) Capture(__VA_ARGS__)\n')
(stage / 'switch.h').write_text(
    '#pragma once\n#include <cstdint>\nextern uint64_t fake_tick;\n'
    'inline uint64_t armGetSystemTick(){return fake_tick;}\n'
    'inline uint64_t armGetSystemTickFreq(){return 1000;}\n')
compiler = root / 'work/toolchain/msys64/mingw64/bin/g++.exe'
env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ['PATH'])
exe = stage / 'test.exe'
subprocess.run([str(compiler), '-std=c++20', '-O2', '-I', str(stage),
                str(root / 'tests/switch_gpu_profile.cpp'), '-o', str(exe)], env=env, check=True)
subprocess.run([str(exe)], env=env, check=True)
