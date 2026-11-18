"""Check cache identity and generic fallback for dump pitch specialization."""
from pathlib import Path
import os,subprocess,re
root=Path(__file__).resolve().parents[1]
sdk_key=(root/'third_party/nfsmw-nx/sdk/include/rex/graphics/vulkan/render_target_cache.h').read_text().split('union DumpPipelineKey {',1)[1].split('DumpPipelineKey()',1)[0]
assert re.findall(r':\s*(\d+)\s*;',sdk_key)==['2','4','1'], 'Pinned SDK dump-key layout changed'
stage=root/'work/analysis/dump-pitch-test';stage.mkdir(parents=True,exist_ok=True)
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2','-I',str(root/'src/platform/switch'),str(root/'tests/switch_dump_pitch.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
