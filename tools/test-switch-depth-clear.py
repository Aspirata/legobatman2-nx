"""Exercise the production clear proof and SDK cutout geometry."""
from pathlib import Path
import ctypes,os,subprocess
root=Path(__file__).resolve().parents[1];stage=root/'work/analysis/depth-clear-cutout-test'
(stage/'rex').mkdir(parents=True,exist_ok=True)
(stage/'rex/logging.h').write_text('#pragma once\n#define REXLOG_INFO(...) ((void)0)\n#define REXLOG_ERROR(...) ((void)0)\n')
if os.name=='nt':ctypes.WinDLL('kernel32').SetErrorMode(0x8003)
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2','-I',str(stage),
    str(root/'tests/switch_depth_clear.cpp'),str(root/'src/platform/switch/depth_clear.cpp'),
    str(root/'src/platform/switch/stencil_zero.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
source=(root/'third_party/nfsmw-nx/sdk/src/graphics/pipeline/render_target/cache.cpp').read_text()
def function(name):
    start=source.index('uint32_t RenderTargetCache::Transfer::'+name+'(')
    at=source.index('{',start)+1;end=at;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
xenos=(root/'third_party/nfsmw-nx/sdk/include/rex/graphics/xenos.h').read_text()
import re
constants={name:int(re.search(r'constexpr uint32_t '+name+r' = (\d+);',xenos)[1])
           for name in ('kEdramTileWidthSamples','kEdramTileHeightSamples','kEdramTileCount')}
prefix='#include <cstdint>\n#include <algorithm>\n#include <cassert>\n#include <cstdio>\n#include <vector>\n'
prefix+='#define assert_true(x) assert(x)\n#define assert_not_zero(x) assert(x)\n'
prefix+='namespace xenos {enum class MsaaSamples:uint32_t{k1X=0,k2X=1,k4X=2};'
prefix+=''.join(f'constexpr uint32_t {n}={v};' for n,v in constants.items())+'}\n'
prefix+='''struct RenderTargetCache { struct Transfer {
struct Rectangle {uint32_t x_pixels,y_pixels,width_pixels,height_pixels;};
static constexpr uint32_t kMaxRectanglesWithoutCutout=3,kMaxCutoutBorderRectangles=4,kMaxRectanglesWithCutout=12;
static uint32_t GetRangeRectangles(uint32_t,uint32_t,uint32_t,uint32_t,xenos::MsaaSamples,bool,Rectangle*,const Rectangle*);
static uint32_t AddRectangle(const Rectangle&,Rectangle*,const Rectangle*);
};};\n'''
main=r'''
int main(){using T=RenderTargetCache::Transfer;T::Rectangle cut{0,0,480,480};unsigned checks=0;
for(unsigned start=720;start<1440;++start)for(unsigned end=start+1;end<=1440;++end){
 assert(T::GetRangeRectangles(start,end,720,12,xenos::MsaaSamples::k4X,false,nullptr,&cut)==0);
 assert(T::GetRangeRectangles(start,end,720,12,xenos::MsaaSamples::k4X,false,nullptr,nullptr)>0);++checks;
}
T::Rectangle out[12];auto n=T::GetRangeRectangles(720,1452,720,12,xenos::MsaaSamples::k4X,false,out,&cut);
assert(n==1&&out[0].y_pixels==480&&out[0].width_pixels==480&&out[0].height_pixels==8);
cut.height_pixels=479;n=T::GetRangeRectangles(720,1440,720,12,xenos::MsaaSamples::k4X,false,out,&cut);
assert(n==1&&out[0].y_pixels==479&&out[0].height_pixels==1);
T::Rectangle zero_source[12],zero_dest[12];
auto zs=T::GetRangeRectangles(720,1440,720,12,xenos::MsaaSamples::k4X,false,zero_source,nullptr);
auto zd=T::GetRangeRectangles(720,1440,720,12,xenos::MsaaSamples::k1X,false,zero_dest,nullptr);
assert(zs==1&&zd==1&&zero_source[0].x_pixels==0&&zero_source[0].y_pixels==0&&zero_dest[0].x_pixels==0&&zero_dest[0].y_pixels==0);
assert(zero_source[0].width_pixels==480&&zero_source[0].height_pixels==480);
assert(zero_dest[0].width_pixels==960&&zero_dest[0].height_pixels==960);
assert(zero_source[0].width_pixels*zero_source[0].height_pixels*4==zero_dest[0].width_pixels*zero_dest[0].height_pixels);
// Compare the optimized aspect plan with a full-import reference at every
// sample. Use SDK-produced geometry, including the untouched40-pixel border.
T::Rectangle full[12],border[12],stencil_cut{0,0,400,240};
auto nf=T::GetRangeRectangles(1718,2048,1718,11,xenos::MsaaSamples::k4X,false,full,nullptr);
auto nb=T::GetRangeRectangles(1718,2048,1718,11,xenos::MsaaSamples::k4X,false,border,&stencil_cut);
assert(nf==1&&full[0].x_pixels==0&&full[0].width_pixels==440&&full[0].height_pixels==240);
assert(nb==1&&border[0].x_pixels==400&&border[0].y_pixels==0&&border[0].width_pixels==40&&border[0].height_pixels==240);
constexpr unsigned sample_count=440*240*4;
auto visit=[](const T::Rectangle* rs,unsigned count,auto fn){
 for(unsigned r=0;r<count;++r)for(unsigned y=rs[r].y_pixels;y<rs[r].y_pixels+rs[r].height_pixels;++y)
  for(unsigned x=rs[r].x_pixels;x<rs[r].x_pixels+rs[r].width_pixels;++x)
   for(unsigned sample=0;sample<4;++sample)fn((y*440+x)*4+sample);
};
for(unsigned ref:{0u,1u}){
 std::vector<uint32_t> source_depth(sample_count),depth(sample_count,0xdeadbeef);
 std::vector<uint8_t> source_stencil(sample_count),stencil(sample_count,0x55);
 for(unsigned p=0;p<sample_count;++p){source_depth[p]=p*2654435761u;source_stencil[p]=uint8_t(p*19+(p/1760)*7);}
 visit(full,nf,[&](unsigned p){depth[p]=source_depth[p];stencil[p]=0;});
 for(unsigned bit=0;bit<8;++bit)visit(border,nb,[&](unsigned p){stencil[p]|=source_stencil[p]&(1u<<bit);});
 auto fallback=stencil;
 visit(full,nf,[&](unsigned p){fallback[p]=0;});
 for(unsigned bit=0;bit<8;++bit)visit(full,nf,[&](unsigned p){fallback[p]|=source_stencil[p]&(1u<<bit);});
 assert(fallback==source_stencil&&depth==source_depth);
 auto reference=source_stencil;
 visit(&stencil_cut,1,[&](unsigned p){reference[p]=ref;stencil[p]=ref;});
 assert(stencil==reference&&depth==source_depth);
}
std::printf("PASS: %u actual SDK range partitions fully overwritten; outside and partial borders preserved\n",checks);
std::puts("PASS: actual SDK400x240 cutout preserves depth/right40x240 at all4 samples, ref0/1 and fallback");
}
'''
oracle=stage/'oracle.cpp';oracle.write_text(prefix+function('GetRangeRectangles')+'\n'+function('AddRectangle')+'\n'+main)
oracle_exe=stage/'oracle.exe'
subprocess.run([str(compiler),'-std=c++20','-O2',str(oracle),'-o',str(oracle_exe)],env=env,check=True)
subprocess.run([str(oracle_exe)],env=env,check=True)

# Cross-component contract: admitted clears have PS=null. Evaluate the actual
# production scheduling predicate so a future SDK change enabling discard
# placeholders for this path fails verification before cutouts are shipped.
pipeline_source=(root/'third_party/nfsmw-nx/sdk/src/graphics/vulkan/pipeline_cache.cpp').read_text()
async_expression=re.search(r'bool use_async = (.*?);',pipeline_source,re.S)[1]
contract=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#define REXCVAR_GET(name) async_enabled
#define VK_NULL_HANDLE nullptr
struct Threads {bool active;bool empty()const{return !active;}};
int main(){int shader,placeholder;
for(bool async_enabled:{false,true})for(bool threads:{false,true})for(bool fragment:{false,true}){
 Threads creation_threads_{threads};void* pixel_shader=nullptr;
 void* placeholder_pixel_shader_=fragment?&placeholder:nullptr;
 bool use_async=EXPRESSION;
 assert(!use_async);
}
std::puts("PASS: actual SDK async predicate excludes PS-null clear pipelines from discard placeholders");
}
'''.replace('#include <cassert>','#include <cassert>\n#include <initializer_list>').replace('EXPRESSION',async_expression)
contract_source=stage/'pipeline_contract.cpp';contract_source.write_text(contract)
contract_exe=stage/'pipeline_contract.exe'
subprocess.run([str(compiler),'-std=c++20','-O2',str(contract_source),'-o',str(contract_exe)],env=env,check=True)
subprocess.run([str(contract_exe)],env=env,check=True)
