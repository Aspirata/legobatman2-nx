"""Run exact Mesa capacity and producer functions with deterministic native append stubs."""
from pathlib import Path
import os,subprocess,re
root=Path(__file__).resolve().parents[1]
mesa=root/'work/toolchain/mesa-source/mesa-switch-1a8c1a66d6fd8d65f10107c4627ffc3606ba5631'
source=(mesa/'src/nouveau/horizon/nouveau_horizon_channel.c').read_text()
stage=root/'work/analysis/native-submit-capacity-test';stage.mkdir(parents=True,exist_ok=True)
def function(name):
 at=source.index('\n'+name+'(');start=source.rfind('\nstatic ',0,at)+1
 body=source.index('{',at);end=body+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]+'\n'
prefix=r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#define GPFIFO_QUEUE_SIZE 2048u
#define NOUVEAU_HORIZON_GPFIFO_FINAL_SKID 4u
#define NOUVEAU_HORIZON_WAIT_SLICE_WORDS 128u
#define NOUVEAU_HORIZON_GM20B_WAIT_WORDS_PER_FENCE 2u
#define GPFIFO_ENTRY_NOT_MAIN 1u
#define GPFIFO_ENTRY_NO_PREFETCH 2u
enum nouveau_horizon_status {NOUVEAU_HORIZON_SUCCESS=0,NOUVEAU_HORIZON_ERROR_NO_SPACE=-7,NOUVEAU_HORIZON_ERROR_DEVICE_LOST=-9,NOUVEAU_HORIZON_ERROR_INVALID_ARGUMENT=-1};
using Result=uint32_t;
#define R_FAILED(x) ((x)!=0)
struct nouveau_horizon_device {bool total_order_channels,enable_timing,lost;};
struct nvioctl_gpfifo_entry {uint64_t value;};
struct Gpu {uint32_t num_entries=0;nvioctl_gpfifo_entry entries[GPFIFO_QUEUE_SIZE]{};};
struct nouveau_horizon_channel {
 nouveau_horizon_device* device=nullptr;Gpu gpu_channel{};
 bool mapped_completion_enabled=false,cache_acquire_emitted=false,pending_work=false;
 uint64_t pending_dwords=0,pending_command_bytes=0,pending_wait_slices=0;
 unsigned next_wait_slice=0;
 uint64_t builtin_addr=0x100000;
 uint32_t cache_acquire_offset_words=0,cache_acquire_words=1,cache_acquire_sync_offset_words=0;
 uint32_t full_barrier_offset_words=0,full_barrier_words=1,full_barrier_acquire_offset_words=0,full_barrier_acquire_words=1;
 struct {nouveau_horizon_status status=NOUVEAU_HORIZON_SUCCESS;} error;
 struct {uint64_t submit_failures=0,waits_enqueued=0,full_barriers_enqueued=0;} stats;
};
struct nouveau_horizon_exec {uint64_t addr=0;uint32_t size_B=0;bool incomplete=false,no_prefetch=false;};
struct nouveau_horizon_fence {uint32_t id,value;};
static bool nouveau_horizon_device_is_lost(nouveau_horizon_device* d){return d->lost;}
static nouveau_horizon_status nouveau_horizon_channel_latch_error(nouveau_horizon_channel* c,nouveau_horizon_status s,Result){return c->error.status=s;}
static nouveau_horizon_status nouveau_horizon_channel_check_error_locked(nouveau_horizon_channel* c){return c->error.status;}
static Result nvGpuChannelAppendEntry(Gpu* g,uint64_t addr,uint32_t,uint32_t,unsigned){if(g->num_entries==GPFIFO_QUEUE_SIZE)return 1;g->entries[g->num_entries++].value=addr;return 0;}
static bool nouveau_horizon_fence_is_valid(const nouveau_horizon_fence* f){return f->id!=0xffffffffu;}
static uint32_t scratch[128];
static uint32_t* nouveau_horizon_channel_wait_slice_cpu(nouveau_horizon_channel*,unsigned){return scratch;}
static uint64_t nouveau_horizon_channel_wait_slice_addr(nouveau_horizon_channel*,unsigned s){return 0x10000+s*512;}
static uint32_t nouveau_horizon_gm20b_build_waits(uint32_t*,uint32_t n,const nouveau_horizon_fence*){return 2*n;}
static nouveau_horizon_status nouveau_horizon_channel_reserve_wait_slice_locked(nouveau_horizon_channel* c,bool order,uint32_t* out){
 if(order){if(c->pending_wait_slices&(1ull<<63))return NOUVEAU_HORIZON_ERROR_NO_SPACE;*out=63;return NOUVEAU_HORIZON_SUCCESS;}
 for(unsigned n=0;n<63;++n){unsigned s=c->next_wait_slice;c->next_wait_slice=(s+1)%63;if(!(c->pending_wait_slices&(1ull<<s))){*out=s;return NOUVEAU_HORIZON_SUCCESS;}}
 return NOUVEAU_HORIZON_ERROR_NO_SPACE;
}
'''
private=(mesa/'src/nouveau/horizon/nouveau_horizon_private.h').read_text()
assert re.search(r'#define NOUVEAU_HORIZON_GPFIFO_FINAL_SKID 4u',private)
parts=[prefix]
if '\nnouveau_horizon_channel_completion_reservation_entries(' in source:parts.append(function('nouveau_horizon_channel_completion_reservation_entries'))
for name in ['nouveau_horizon_channel_reserve_entries_locked','nouveau_horizon_channel_append_locked','nouveau_horizon_channel_append_waits_locked','nouveau_horizon_channel_emit_cache_acquire_locked','nouveau_horizon_channel_full_barrier_locked','nouveau_horizon_channel_exec_locked']:parts.append(function(name))
parts.append('\n#include "'+(root/'tests/switch_native_submit_capacity.cpp').as_posix()+'"\n')
test=stage/'test.cpp';test.write_text('\n'.join(parts))
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2',str(test),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
