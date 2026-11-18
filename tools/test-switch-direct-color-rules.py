"""Check direct resolve guards and tile math against the exact SDK CPU oracle."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];stage=root/'work/analysis/direct-color-rules-test';stage.mkdir(parents=True,exist_ok=True)
source=(root/'third_party/nfsmw-nx/sdk/src/graphics/pipeline/texture/util.cpp').read_text()
start=source.index('int32_t GetTiledOffset2D(');body=source.index('{',start);depth=1;end=body+1
while depth:
 depth+=(source[end]=='{')-(source[end]=='}');end+=1
oracle=source[start:end].replace('GetTiledOffset2D','SdkTiledOffset')
prefix='#include <cstdint>\nnamespace rex {template<class T> constexpr T align(T x,T a){return (x+a-1)&~(a-1);}}\nnamespace xenos {constexpr uint32_t kTextureTileWidthHeight=32;}\n'
test=stage/'test.cpp';test.write_text(prefix+oracle+'\n#include "'+(root/'tests/switch_direct_color_rules.cpp').as_posix()+'"\n')
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2',str(test),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
