"""Run the production bounded causal recorder on the host."""
from pathlib import Path
import os
import subprocess
import ctypes
import importlib.util
import struct

if os.name=='nt':
    ctypes.WinDLL('kernel32').SetErrorMode(0x8003)

root=Path(__file__).resolve().parents[1]
stage=root/'work/analysis/clear-causal-trace-test'
stage.mkdir(parents=True,exist_ok=True)
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
exe=stage/'test.exe'
subprocess.run([str(compiler),'-std=c++20','-O2',str(root/'tests/switch_clear_trace.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],cwd=stage,env=env,check=True)
spec=importlib.util.spec_from_file_location('clear_trace_reader',root/'tools/read-switch-clear-trace.py')
reader=importlib.util.module_from_spec(spec); spec.loader.exec_module(reader)
result=reader.read_trace(stage/'fixture.bin')
assert result['complete'] and result['frame']==180 and result['operations']==1
assert result['events'][2]['float_values']==[1.0,0.0,1.0,1.0]
raw=(stage/'fixture.bin').read_bytes()
for malformed in (raw[:-1],raw[:-4],raw+b'\x00'*4,raw[:20]+struct.pack('<I',999)+raw[24:]):
    bad=stage/'malformed.bin'; bad.write_bytes(malformed)
    try: reader.read_trace(bad)
    except ValueError: pass
    else: raise AssertionError('Malformed trace must never be treated as complete evidence')
print('PASS: production trace decoder, exact float bits and rejection of truncated or inconsistent evidence')
