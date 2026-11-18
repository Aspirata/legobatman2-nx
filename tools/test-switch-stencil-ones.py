"""Check stencil split cache identity and query-safe draw scope."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1]
stage=root/'work/analysis/stencil-ones-test';stage.mkdir(parents=True,exist_ok=True)
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2','-pthread','-I',str(root/'src/platform/switch'),str(root/'tests/switch_stencil_ones.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
