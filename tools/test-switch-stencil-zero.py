"""Exercise production zero-stencil generation reuse and full-clear publication."""
from pathlib import Path
import ctypes,os,subprocess
root=Path(__file__).resolve().parents[1];stage=root/'work/analysis/stencil-zero-reuse-test'
(stage/'rex').mkdir(parents=True,exist_ok=True)
(stage/'rex/logging.h').write_text('#pragma once\n#define REXLOG_INFO(...) ((void)0)\n#define REXLOG_ERROR(...) ((void)0)\n')
if os.name=='nt':ctypes.WinDLL('kernel32').SetErrorMode(0x8003)
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2','-I',str(stage),
 str(root/'tests/switch_stencil_zero.cpp'),str(root/'src/platform/switch/depth_clear.cpp'),
 str(root/'src/platform/switch/stencil_zero.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
