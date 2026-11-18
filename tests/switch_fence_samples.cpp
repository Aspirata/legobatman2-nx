#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
uint64_t fake_tick=0;
std::vector<std::string> summary;
std::vector<std::vector<std::string>> groups;
template<typename T> std::string Text(T value){std::ostringstream s;s<<value;return s.str();}
template<typename... Args> void FenceLog(const char* format,Args... values){
 std::vector<std::string> v{Text(values)...};
 if(std::string(format).starts_with("LB2 fence samples:"))summary=v;
 else if(std::string(format).starts_with("LB2 fence sample group:"))groups.push_back(v);
}
#include "../src/platform/switch/fence_samples.cpp"
struct Fake { unsigned calls=0; Lb2FenceDrain result=Lb2FenceDrain::kComplete; };
Lb2FenceDrain Drain(void* p){auto& f=*static_cast<Fake*>(p);++f.calls;fake_tick+=5;return f.result;}
int main(int argc,char** argv){
 auto check=[](bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}};
 Lb2FenceKey k;Fake f;
 if(argc==2&&std::string(argv[1])=="groups"){
  {Lb2FenceSample first(k,true,Drain,&f);}
  fake_tick=11000;k.kind=1;
  for(unsigned group=0;group<160;++group){
   k.vs=group;fake_tick+=125;bool observed=false;
   for(unsigned i=0;i<10000&&!observed;++i){Lb2FenceSample s(k,true,Drain,&f);
    if(s.active()){observed=true;Lb2FenceSampleGuestDraw(group+1);s.Commit();}}
   check(observed,"Every test operation must receive a real isolated sample");
  }
  Lb2FenceSamplesReport();
  check(summary[1]=="160"&&summary[5]=="32"&&groups.size()==16,"Group overflow preserves full valid totals and bounded reports");
  std::puts("PASS:128-group capacity, complete overflow totals and16-line report limit");return 0;
 }
 if(argc==2&&std::string(argv[1])=="budget"){
  {Lb2FenceSample first(k,true,Drain,&f);}
  f.result=Lb2FenceDrain::kUnsafe;fake_tick=11000;
  for(unsigned i=0;i<200000;++i){fake_tick+=125;Lb2FenceSample s(k,true,Drain,&f);s.Commit();}
  check(f.calls==256,"Unsafe and discarded admissions must consume the256-attempt lifetime budget");
  std::puts("PASS:256-attempt process limit also bounds unsuccessful synchronization attempts");return 0;
 }
 if(argc==2&&std::string(argv[1])=="postfail"){
  {Lb2FenceSample first(k,true,Drain,&f);}
  fake_tick=11000;bool got_sample=false;
  for(unsigned i=0;i<10000&&!got_sample;++i){Lb2FenceSample s(k,true,Drain,&f);
   if(s.active()){got_sample=true;f.result=Lb2FenceDrain::kFailed;s.Commit();}}
  check(got_sample&&f.calls==2,"Completion failure invokes exactly the two planned drains");
  Lb2FenceSampleGuestDraw(123);fake_tick+=100000;
  for(unsigned i=0;i<10000;++i){Lb2FenceSample s(k,true,Drain,&f);s.Commit();}
  Lb2FenceSamplesReport();
  check(f.calls==2&&groups.empty(),"Failed completion clears TLS, emits no valid latency and disables future waits");
  std::puts("PASS: failed post-drain discards result, clears ownership and prevents future waits");return 0;
 }
 {Lb2FenceSample s(k,false,Drain,&f);s.Commit();}
 check(f.calls==0,"Disabled profiling must never invoke a drain");
 for(unsigned i=0;i<1000;++i){Lb2FenceSample s(k,true,Drain,&f);s.Commit();}
 check(f.calls==0,"Warmup must not synchronize the GPU");
 fake_tick=11000;
 bool got=false;
 for(unsigned i=0;i<10000&&!got;++i){
  Lb2FenceSample s(k,true,Drain,&f);
  if(s.active()){
   got=true;const auto before=f.calls;
   {Lb2FenceSample nested(k,true,Drain,&f);nested.Commit();}
   check(f.calls==before,"Nested operations must not synchronize or steal workload ownership");
   Lb2FenceSampleGuestDraw(40);Lb2FenceSampleTransfer(1000);fake_tick+=20;s.Commit();
  }
 }
 check(got&&f.calls==2,"A committed sample must drain exactly once before and after the operation");
 Lb2FenceSampleGuestDraw(99);Lb2FenceSampleTransfer(999);
 Lb2FenceSamplesReport();
 check(groups.size()==1,"A valid operation must produce one bounded group");
 check(groups[0][8]=="1"&&groups[0][9]=="25"&&groups[0][10]=="20"&&groups[0][11]=="5", "Prior drain is excluded from completion; preparation and final drain remain separate");
 check(groups[0][14]=="1"&&groups[0][15]=="40"&&groups[0][16]=="1"&&groups[0][17]=="1000","Only work inside the isolated sample is attributed");
 groups.clear();Lb2FenceSamplesReport();check(groups.empty(),"Reports reset completed groups");
 const auto admitted=f.calls;
 for(unsigned i=0;i<20000;++i){Lb2FenceSample s(k,true,Drain,&f);s.Commit();}
 check(f.calls==admitted,"125ms spacing prevents excessive synchronization");
 fake_tick+=1000;
 for(unsigned i=0;i<10000;++i){Lb2FenceSample s(k,true,Drain,&f);if(s.active())break;}
 const auto after_cancel=f.calls;
 check(after_cancel==admitted+1,"Uncommitted operations have no post-drain");
 fake_tick+=1000;f.result=Lb2FenceDrain::kUnsafe;
 for(unsigned i=0;i<10000&&f.calls==after_cancel;++i){Lb2FenceSample s(k,true,Drain,&f);s.Commit();}
 check(f.calls==after_cancel+1,"Unsafe admission performs no post-drain");
 fake_tick+=1000;f.result=Lb2FenceDrain::kFailed;
 const auto before_fail=f.calls;
 for(unsigned i=0;i<10000&&f.calls==before_fail;++i){Lb2FenceSample s(k,true,Drain,&f);s.Commit();}
 const auto failed=f.calls;fake_tick+=100000;
 for(unsigned i=0;i<10000;++i){Lb2FenceSample s(k,true,Drain,&f);s.Commit();}
 check(failed==before_fail+1&&f.calls==failed,"Failed waits permanently stop sampling without retries or new waits");
 std::puts("PASS: sparse sampling, warmup, both drains, cancellation, TLS ownership, bounds, failure and reset");
}
