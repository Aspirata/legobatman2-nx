"""Exercise intrusive operation recorder with deterministic host clock/drains."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];stage=root/'work/analysis/fence-samples-test'
(stage/'rex').mkdir(parents=True,exist_ok=True)
(stage/'rex/logging.h').write_text('#pragma once\n#define REXLOG_INFO(...) FenceLog(__VA_ARGS__)\n#define REXLOG_WARN(...) FenceLog(__VA_ARGS__)\n')
(stage/'switch.h').write_text('#pragma once\n#include <cstdint>\nextern uint64_t fake_tick;\ninline uint64_t armGetSystemTick(){return fake_tick;}\ninline uint64_t armGetSystemTickFreq(){return 1000;}\n')
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2','-I',str(stage),str(root/'tests/switch_fence_samples.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
subprocess.run([str(exe),'budget'],env=env,check=True)
subprocess.run([str(exe),'postfail'],env=env,check=True)
subprocess.run([str(exe),'groups'],env=env,check=True)
