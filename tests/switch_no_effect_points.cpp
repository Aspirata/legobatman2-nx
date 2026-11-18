#include <cstdio>
#include <cstdlib>
#include <array>
#include "../src/platform/switch/lb2_no_effect_points.h"
std::array<uint64_t,5> totals{};
template<typename... Args> void PointsLog(const char*,Args... values){totals={uint64_t(values)...};}
#include "../src/platform/switch/no_effect_points.cpp"
int main(){
 unsigned checks=0;
 auto check=[&](bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}};
 using namespace lb2::points;
 State good;good.vs=kVertexShader;good.point_list=good.auto_index=good.normal_vertex_path=good.normal_export=true;
 check(CanElide(good),"Known point with no stored output and no live query must be eligible");
 good.has_ps=true;good.ps=kPixelShader;
 check(CanElide(good),"Known constant-color PS with all color writes masked is also eligible");
 auto bad=good;bad.vs^=1;check(!CanElide(bad),"Unknown vertex program must retain the original path");
 bad=good;bad.ps^=1;check(!CanElide(bad),"Unknown pixel program must retain the original path");
 for(unsigned bit=0;bit<32;++bit){
  bad=good;bad.depth_control=uint32_t(1)<<bit;check(!CanElide(bad),"Any changed depth/stencil state fails closed");
  bad=good;bad.color_mask=uint32_t(1)<<bit;check(!CanElide(bad),"Every color output bit must block elimination");
 }
 for(unsigned mask=1;mask<16;++mask){
  bad=good;bad.vertex_memexport=mask&1;bad.pixel_memexport=mask&2;bad.query_active=mask&4;bad.normal_export=!(mask&8);
  check(!CanElide(bad),"Memory exports, live queries and non-normal exports are preserved, including combinations");
 }
 bad=good;bad.point_list=false;check(!CanElide(bad),"Non-point primitives retain the original path");
 bad=good;bad.auto_index=false;check(!CanElide(bad),"DMA and immediate indices retain the original path");
 bad=good;bad.normal_vertex_path=false;check(!CanElide(bad),"Explicit/tessellation paths retain the original path");
 check(!Lb2TryNoEffectPoint(&good,false),"Startup-disabled optimization must never remove even eligible calls");
 check(Lb2TryNoEffectPoint(&good,true),"Startup-enabled optimization may remove eligible calls");
 bad=good;bad.query_active=true;check(!Lb2TryNoEffectPoint(&bad,true),"Active query calls remain emitted");
 bad=good;bad.has_ps=false;check(!Lb2TryNoEffectPoint(&bad,true),"Inconsistent absent-PS state fails closed");
 check(!Lb2TryNoEffectPoint(nullptr,true),"Missing state is never eligible");
 Lb2NoEffectPointsReport();
 check(totals==std::array<uint64_t,5>{4,2,1,1,1},"Candidate, eligible, elided and blocked calls are distinct");
 Lb2NoEffectPointsReport();
 check(totals==std::array<uint64_t,5>{},"Elision counters reset for the next interval");
 std::printf("PASS: %u no-effect point eligibility and side-effect rejection checks\n",checks);
}
