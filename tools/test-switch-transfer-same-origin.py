"""Validate the scoped exact stencil address specialization."""
from pathlib import Path
import os,subprocess
r=Path(__file__).resolve().parents[1]
stage=r/'work/analysis/transfer-same-origin-test';stage.mkdir(parents=True,exist_ok=True)
c=r/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(c.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(c),'-std=c++20','-O2','-I',str(r/'src/platform/switch'),str(r/'tests/switch_transfer_same_origin.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
