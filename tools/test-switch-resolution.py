"""Exercise title resolution hooks against the actual generated video-mode code."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
stage = root / 'work/analysis/switch-resolution'
(stage / 'rex').mkdir(parents=True, exist_ok=True)
(stage / 'legobatman2_pch.h').write_text('#pragma once\n')
(stage / 'rex/kernel/xboxkrnl').mkdir(parents=True, exist_ok=True)
(stage / 'rex/kernel/xboxkrnl/video.h').write_text('#pragma once\n')
for name in ('hook.h', 'cvar.h', 'logging.h'):
    (stage / 'rex' / name).write_text('#pragma once\n')

def function(unit, name):
    text = (root / f'generated/default/legobatman2_recomp.{unit}.cpp').read_text()
    start = text.index(f'DEFINE_REX_FUNC({name})')
    end = text.find('\nDEFINE_REX_FUNC(', start + 1)
    return text[start:end if end >= 0 else len(text)]

originals = function(147, 'sub_824B0D70') + function(9, 'sub_824B0C90')
prelude = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <atomic>
union Reg { uint64_t u64=0; int64_t s64; uint32_t u32; int32_t s32; };
struct CR { bool eq=false,gt=false; template<class T> void compare(T a,T b,int) {eq=a==b;gt=a>b;} };
struct PPCContext { Reg r1,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r31; uint64_t lr=0; CR cr6; int xer=0; };
std::unordered_map<uint32_t,uint64_t> mem;
#define REX_LOAD_U32(a) uint32_t(mem[uint32_t(a)])
#define REX_STORE_U32(a,v) (mem[uint32_t(a)]=uint32_t(v))
#define REX_LOAD_U64(a) mem[uint32_t(a)]
#define REX_STORE_U64(a,v) (mem[uint32_t(a)]=uint64_t(v))
#define REX_FUNC(n) void n(PPCContext& ctx,uint8_t* base)
#define REX_EXTERN(n) extern "C" REX_FUNC(n)
#define REX_HOOK_RAW(n) extern "C" REX_FUNC(n)
#define DEFINE_REX_FUNC(n) extern "C" REX_FUNC(__imp__##n)
#define REX_FUNC_PROLOGUE()
#define REXLOG_INFO(...) ((void)0)
// Raw defaults can differ from the effective mode selected by the SDK preset.
namespace rex::cvar { std::string GetFlagByName(std::string_view n) { return n=="video_mode_width"?"1280":"720"; } }
namespace rex::system { struct X_VIDEO_MODE { uint32_t display_width,display_height; }; }
namespace rex::kernel::xboxkrnl { void VdQueryVideoMode(rex::system::X_VIDEO_MODE* m) {m->display_width=848;m->display_height=480;} }
REX_EXTERN(__imp__XGetVideoMode) {
  for(unsigned i=0;i<48;i+=4) REX_STORE_U32(ctx.r3.u32+i,0x100+i);
  REX_STORE_U32(ctx.r3.u32,848); REX_STORE_U32(ctx.r3.u32+4,480);
}
unsigned seen_w,seen_h; uint64_t seen_lr;
unsigned overlay_w=1280,overlay_h=720;
extern "C" void RexSwitchPerfResolution(unsigned w,unsigned h) {overlay_w=w;overlay_h=h;}
extern "C" void Lb2GpuProfileFrame() {}
REX_EXTERN(__imp__sub_824B0B08) {seen_w=ctx.r3.u32;seen_h=ctx.r4.u32;seen_lr=ctx.lr;ctx.r3.u64=0xABCD;}
REX_EXTERN(__imp__sub_824B0B70) {__imp__sub_824B0B08(ctx,base);}
REX_EXTERN(__real___imp__VdSwap) {}
'''
hooks = root / 'src/platform/switch/resolution.cpp'
if hooks.exists():
    implementation = f'\n#include "{hooks.as_posix()}"\n'
else:
    implementation = '\n'.join(f'REX_EXTERN({name}) {{ __imp__{name}(ctx,base); }}' for name in
        ('sub_824B0D70', 'sub_824B0C90', 'sub_824B0B08', 'sub_824B0B70'))
tests = r'''
int main() {
 int errors=0;
 auto check=[&](bool ok,const char* what){std::printf("%s: %s\n",what,ok?"PASS":"FAIL");errors+=!ok;};
 uint8_t* base=nullptr;
 for(auto fn:{sub_824B0D70,sub_824B0C90}) {
  PPCContext ctx;ctx.r1.u64=0x8000;ctx.r3.u64=0x1000;ctx.r31.u64=0x777;ctx.lr=0x1234;
  // C90 also has a legacy global override: exercise its 720p branch.
  REX_STORE_U32(0x82D75170+2156,1280);REX_STORE_U32(0x82D75170+2160,720);
  fn(ctx,base);
  check(overlay_w==848 && overlay_h==480,"Overlay publishes effective internal resolution");
  check(REX_LOAD_U32(0x1000)==848 && REX_LOAD_U32(0x1004)==480,"Game mode honors 480p after original hardcoded override");
  check(REX_LOAD_U32(0x1010)==0 && REX_LOAD_U32(0x1014)==0x114,"HD flag and refresh field remain consistent");
  check(ctx.r1.u64==0x8000 && ctx.r31.u64==0x777 && ctx.lr==0x1234,"Guest stack, LR and nonvolatile register preserved");
 }
 for(auto fn:{sub_824B0B08,sub_824B0B70}) {
  for(uint64_t lr:{0x824171FC,0x82417228,0x82417264,0x824172A0,0x824172D0}) {
   PPCContext ctx;ctx.r3.u64=1280;ctx.r4.u64=720;ctx.lr=lr;fn(ctx,base);
   check(seen_w==848 && seen_h==480 && seen_lr==lr && ctx.r3.u64==0xABCD,"Full-size scene surface resized before original allocation");
  }
  for(auto dims:{std::pair{640u,360u},std::pair{960u,800u},std::pair{1280u,608u}}) {
   PPCContext ctx;ctx.r3.u64=dims.first;ctx.r4.u64=dims.second;ctx.lr=0x82417338;fn(ctx,base);
   check(seen_w==dims.first && seen_h==dims.second,"Other effects, shadows and split targets untouched");
  }
  PPCContext ctx;ctx.r3.u64=1280;ctx.r4.u64=720;ctx.lr=0xDEADBEEF;fn(ctx,base);
  check(seen_w==1280 && seen_h==720,"Unrelated caller not resized by dimensions alone");
 }
 return errors?1:0;
}
'''
(stage / 'test.cpp').write_text(prelude + originals + implementation + tests)
compiler = root / 'work/toolchain/msys64/mingw64/bin/g++.exe'
env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ['PATH'])
exe = stage / 'test.exe'
subprocess.run([str(compiler), '-std=c++20', '-O2', '-I', str(stage), str(stage / 'test.cpp'), '-o', str(exe)], check=True, env=env)
raise SystemExit(subprocess.run([str(exe)], env=env).returncode)
