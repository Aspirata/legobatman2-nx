"""Exercise native queue observation with the exact pinned driver's public ABI."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
mesa=root/'work/toolchain/mesa-source/mesa-switch-1a8c1a66d6fd8d65f10107c4627ffc3606ba5631'
stage=root/'work/analysis/switch-native-queue-profile-test'
(stage/'rex').mkdir(parents=True,exist_ok=True)
(stage/'rex/logging.h').write_text('#pragma once\n#define REXLOG_INFO(...) NativeLog(__VA_ARGS__)\n')
(stage/'switch.h').write_text('#pragma once\n#include <cstdint>\nextern uint64_t fake_tick;\ninline uint64_t armGetSystemTick(){return fake_tick;}\ninline uint64_t armGetSystemTickFreq(){return 1000;}\n')
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2','-I',str(stage),'-I',str(mesa/'src'),'-I',str(mesa/'include'),str(root/'tests/switch_native_queue_profile.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
