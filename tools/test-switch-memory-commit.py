"""Exercise the actual Horizon commit function with a bounded mapping backend."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
sdk = root / 'third_party/nfsmw-nx/sdk'
source = (sdk / 'src/core/guest_memory_switch.cpp').read_text()
start = source.index('bool RexGmCommit(')
end = source.index('bool RexGmFaultIn(', start)
lookup = source[source.index('Chunk* ChunkAt('):source.index('/* The view that', source.index('Chunk* ChunkAt('))]
stage = root / 'work/analysis/switch-memory-commit'
stage.mkdir(parents=True, exist_ok=True)
harness = r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <map>
#include <mutex>
using u64 = uint64_t;
using Result = uint32_t;
using RexGmAccess = int;
constexpr int REX_GM_WRITE=3;
constexpr uint32_t REX_GM_ERR_NOMEM=1, REX_GM_ERR_NOCODEVA=2, Perm_Rw=3;
constexpr size_t kPageSize=0x1000, kBlockSize=0x200000;
constexpr size_t AlignDown(size_t v,size_t a) {return v & ~(a-1);}
constexpr size_t AlignUp(size_t v,size_t a) {return (v+a-1) & ~(a-1);}
struct Chunk {size_t offset,length; void* backing; void* shadow;};
struct State {
 std::mutex mutex; uint8_t* base=reinterpret_cast<uint8_t*>(0x100000000ull);
 size_t size=0x120000000ull, committed=0; uint32_t last_result=0;
 std::map<size_t,Chunk> chunks;
};
State state;
State& S() {return state;}
int maps=0, allocations=0, max_maps=128;
bool reject_permission=false;
void* TestAlloc(size_t align,size_t length) {++allocations; return _aligned_malloc(length,align);}
void TestFree(void* p) {--allocations; _aligned_free(p);}
bool R_FAILED(Result r) {return r!=0;}
int Proc() {return 1;}
void virtmemLock() {}
void virtmemUnlock() {}
void* virtmemFindCodeMemory(size_t,size_t) {return reinterpret_cast<void*>(0x200000000ull);}
Result svcMapProcessCodeMemory(int,u64,u64,size_t) {if(maps==max_maps) return 0xCE01; ++maps; return 0;}
Result svcUnmapProcessCodeMemory(int,u64,u64,size_t) {--maps; return 0;}
Result svcSetProcessMemoryPermission(int,u64,size_t,int) {return reject_permission ? 0xCE01 : 0;}
''' + lookup + r'''
#define memalign TestAlloc
#define free TestFree
''' + source[start:end] + r'''
#undef memalign
#undef free
void reset() {
 for(auto& [_,c]:state.chunks) TestFree(c.backing);
 state.chunks.clear(); state.committed=0; state.last_result=0;
 maps=0; reject_permission=false;
}
int main() {
 int failures=0;
 auto check=[&](bool ok,const char* name) {std::printf("%s: %s\n",name,ok?"PASS":"FAIL"); failures+=!ok;};
 bool ok=true;
 for(size_t i=0;i<1024;i++) if(!RexGmCommit(0x30000000+i*kPageSize,kPageSize,REX_GM_WRITE)) {ok=false;break;}
 check(ok && maps==2 && state.committed==4*1024*1024,"1024 small guest commits stay within mapping budget");
 reset();
 RexGmCommit(0x30001000,kPageSize,REX_GM_WRITE);
 auto* c=ChunkAt(state,0x30001000); auto* byte=static_cast<uint8_t*>(c->backing)+0x1000;
 *byte=0xA5;
 check(RexGmCommit(0x30001000,kPageSize,REX_GM_WRITE) && *byte==0xA5 && maps==1,"Repeated commit preserves guest data");
 reset();
 check(RexGmCommit(state.size-kPageSize,kPageSize,REX_GM_WRITE) && state.chunks.rbegin()->second.offset+state.chunks.rbegin()->second.length<=state.size,"Last window page stays in bounds");
 reset(); reject_permission=true;
 check(!RexGmCommit(0x30001000,kPageSize,REX_GM_WRITE) && maps==0 && allocations==0 && state.chunks.empty(),"Failed permission change releases backing and alias");
 reset();
 return failures ? 1 : 0;
}
'''
(stage / 'test.cpp').write_text(harness)
compiler = root / 'work/toolchain/msys64/mingw64/bin/g++.exe'
exe = stage / 'test.exe'
env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ['PATH'])
subprocess.run([str(compiler), '-std=c++20', '-O2', str(stage / 'test.cpp'), '-o', str(exe)], env=env, check=True)
raise SystemExit(subprocess.run([str(exe)], env=env).returncode)
