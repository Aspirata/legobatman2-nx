#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
std::vector<std::string> summary;
std::vector<std::vector<std::string>> groups;
template<typename T> std::string AsText(T v){std::ostringstream s;s<<v;return s.str();}
template<typename... Args> void DrawLog(const char* format,Args... args){
  std::vector<std::string> values{AsText(args)...};
  if(std::string_view(format).starts_with("LB2 guest draws:"))summary=values;
  else if(std::string_view(format).starts_with("LB2 guest draw group:"))groups.push_back(values);
}
extern "C" void Lb2FenceSampleGuestDraw(uint32_t) {}
#include "../src/platform/switch/draw_profile.cpp"
int main(){
  auto check=[](bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}};
  Lb2GuestDrawKey k;k.vs=7;k.ps=11;k.width=100000;k.height=100000;k.flags=3;
  Lb2GpuProfileDraw(&k,100);
  auto other=k;other.x=80;other.flags=7;other.depth=99;
  Lb2GpuProfileDraw(&other,6);
  Lb2GpuProfileDraw(nullptr,99);
  Lb2GpuProfileDrawsReport();
  check(summary[0]=="2"&&summary[1]=="106","Only real draw observations contribute complete draw/vertex totals");
  check(summary[2]=="20000000000","Scissor area must use64-bit multiplication");
  check(summary[3]=="2"&&summary[4]=="2"&&summary[6]=="2"&&groups.size()==2,"Side effects and distinct pass state must survive grouping");
  groups.clear();Lb2GpuProfileDrawsReport();
  check(summary[0]=="0"&&summary[2]=="0"&&groups.empty(),"Intervals must reset without replaying old passes");
  std::vector<std::thread> threads;
  for(int i=0;i<4;++i)threads.emplace_back([&]{for(int j=0;j<1000;++j)Lb2GpuProfileDraw(&k,10);});
  for(auto& t:threads)t.join();
  Lb2GpuProfileDrawsReport();
  check(summary[0]=="4000"&&summary[1]=="40000"&&groups.size()==1,"Concurrent producers must not lose or duplicate observations");
  groups.clear();
  for(unsigned i=0;i<700;++i){k.vs=i;Lb2GpuProfileDraw(&k,10);}
  Lb2GpuProfileDrawsReport();
  check(summary[0]=="700"&&summary[1]=="7000"&&summary[2]=="7000000000000","Group overflow must retain complete totals");
  check(summary[5]=="188"&&summary[6]=="512"&&groups.size()<=24,"Overflow and log bounds must be explicit");
  std::puts("PASS: emitted draw totals,64-bit area, pass separation, interval reset, concurrent producers and bounded overflow");
}
