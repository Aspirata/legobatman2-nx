#include "lb2_stencil_copy_rules.h"
#include <cstdio>
#include <cstdlib>

int main(){
  using namespace lb2::stencil;
  unsigned checks=0;
  auto check=[&](bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}};
  Target source{1718,11,0,2,true},dest{1718,11,0,0,true};
  check(Eligible(true,source,dest,1,1,true),"Measured same-layout depth4x to depth1x qualifies");
  check(!Eligible(false,source,dest,1,1,true),"Disabled trial preserves baseline");
  check(!Eligible(true,dest,source,1,1,true),"Opposite direction must preserve old MSAA path");
  check(!Eligible(true,source,dest,2,1,true)&&!Eligible(true,source,dest,1,2,true),"Scaled draws must retain baseline addressing");
  check(!Eligible(true,source,dest,1,1,false),"Nonstandard samples cannot use the fixed sample map");
  for(unsigned field=0;field<5;++field){
    auto other=dest;
    if(field==0)++other.base;if(field==1)++other.pitch;if(field==2)++other.format;
    if(field==3)other.samples=1;if(field==4)other.depth=false;
    check(!Eligible(true,source,other,1,1,true),"Different ownership layouts must fall back");
  }
  Plan plan{};
  check(MakePlan(3,5,7,9,880,2992,440,1496,4*1024*1024,plan),"Odd subrectangle qualifies");
  check(plan.push.row_words==2&&plan.bytes==72&&plan.groups==1,"Padded rows and workgroup count are exact");
  check(!MakePlan(0,0,0,9,880,2992,440,1496,4*1024*1024,plan),"Empty regions cannot emit GPU work");
  check(!MakePlan(UINT32_MAX,0,2,9,880,2992,440,1496,4*1024*1024,plan),"Overflowing origin must fall back");
  check(!MakePlan(879,0,2,9,880,2992,440,1496,4*1024*1024,plan),"Destination bounds must hold");
  check(!MakePlan(878,0,2,9,880,2992,439,1496,4*1024*1024,plan),"Source bounds must hold after sample mapping");
  check(!MakePlan(0,0,880,2992,880,2992,440,1496,100,plan),"Staging capacity exhaustion must preserve original path");
  check(!MakePlan(160,2976,1,1,880,2992,440,1496,4*1024*1024,plan),"Padded EDRAM tile2048 must retain the SDK wrap path");
  check(MakePlan(0,2976,160,16,880,2992,440,1496,4*1024*1024,plan),"Valid tiles2046 and2047 remain eligible");
  check(MakePlan(0,0,880,2976,880,2992,440,1496,4*1024*1024,plan),"Complete unwrapped rows fit bounded staging");
  check(plan.bytes==2618880&&plan.groups==10230,"Clamped surface size is exact");
  std::printf("PASS: %u guarded stencil eligibility, bounds, padding and capacity checks\n",checks);
}
