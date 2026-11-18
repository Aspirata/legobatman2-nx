#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
std::vector<std::string> summary;
std::vector<std::vector<std::string>> groups;
template<typename T> std::string AsText(T v){std::ostringstream s;s<<v;return s.str();}
template<typename... Args> void TransferLog(const char* format,Args... args){
  std::vector<std::string> values{AsText(args)...};
  if(std::string_view(format).starts_with("LB2 transfers:"))summary=values;
  else groups.push_back(values);
}
extern "C" void Lb2FenceSampleTransfer(uint64_t) {}
#include "../src/platform/switch/transfer_profile.cpp"
int main(){
  auto check=[](bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}};
  Lb2GpuProfileTransfer(1,2,3,0,2,100);
  for(unsigned i=0;i<7;++i)Lb2GpuProfileTransfer(1,2,3,0,2,100);
  for(unsigned i=0;i<8;++i)Lb2GpuProfileTransfer(4,5,6,2,1,50);
  Lb2GpuProfileFastStencilCopy(3,64);
  Lb2GpuProfileTransfersReport();
  check(summary[0]=="8"&&summary[1]=="0"&&summary[2]=="8","Color and stencil draws must be counted separately");
  check(summary[3]=="24"&&summary[4]=="1200","Repeated bit draws must contribute rectangles and pixels");
  check(groups.size()==2,"Equal transfer identities must aggregate into bounded groups");
  check(summary[6]=="1"&&summary[7]=="3"&&summary[8]=="64","Fast replacements must be reported separately from original draws");
  groups.clear();Lb2GpuProfileTransfersReport();
  check(summary[0]=="0"&&summary[2]=="0"&&groups.empty(),"A new interval must not repeat old draws");
  for(unsigned i=0;i<200;++i)Lb2GpuProfileTransfer(i,2,3,1,1,10);
  Lb2GpuProfileTransfersReport();
  check(summary[1]=="200"&&summary[4]=="2000","Group exhaustion must retain complete totals");
  check(std::stoull(summary[5])>0&&groups.size()<=16,"Exhaustion and log bounds must be explicit");
  std::puts("PASS: transfer types, grouping, repeated stencil area, reset and bounded overflow reporting");
}
