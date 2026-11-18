"""Check observational transfer accounting independently of GPU execution."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1];stage=root/'work/analysis/switch-transfer-profile-test'
(stage/'rex').mkdir(parents=True,exist_ok=True)
(stage/'rex/logging.h').write_text('#pragma once\n#define REXLOG_INFO(...) TransferLog(__VA_ARGS__)\n')
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2','-I',str(stage),str(root/'tests/switch_transfer_profile.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
