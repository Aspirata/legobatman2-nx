#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../src/platform/switch/lb2_clear_trace.h"

void Check(bool condition, const char* message) {
  if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
int main() {
  using namespace lb2::clear_trace;
  std::array<uint32_t,192> fetch;
  for (unsigned i=0;i<fetch.size();++i) fetch[i]=i+1000;
  std::array<uint32_t,225> bindings;
  Check(PackTextureBindings(0,fetch.data(),bindings)==1 && bindings[0]==0,"empty texture mask reads no slots");
  Check(PackTextureBindings(0x80000001u,fetch.data(),bindings)==15 &&
        bindings[1]==0 && bindings[7]==1005 && bindings[8]==31 && bindings[14]==1191,
        "sparse first/last fetch slots preserve exact six-word descriptors");
  Check(PackTextureBindings(0xffffffffu,fetch.data(),bindings)==225 && bindings.back()==1191,
        "all32 fetch descriptors fit exactly in the bounded payload");
  Recorder trace(64);
  Check(!trace.BeginOperation(180,false), "disabled capture must be inert");
  Check(trace.words().empty(), "disabled capture must record nothing");
  Check(!trace.BeginOperation(179,true), "warmup frame must be inert");
  Check(trace.BeginOperation(180,true), "first selected frame begins");
  trace.Record(Kind::kExtent, {480,480,1});
  Check(trace.BeginOperation(180,true), "same frame continues");
  trace.Record(Kind::kOwnership, {0x1234,0,1024,0});
  auto h=trace.header();
  Check(h[0]==0x5443324c && h[1]==1 && h[2]==180 && h[3]==0,
        "header must identify format and GPU frame");
  Check(h[4]==2 && h[5]==4 && h[6]==0 && h[7]==19,
        "operations and records must preserve causal order");
  const std::vector<uint32_t> expected={
    uint32_t(Kind::kOperation),0,1,
    uint32_t(Kind::kExtent),3,1,480,480,1,
    uint32_t(Kind::kOperation),0,2,
    uint32_t(Kind::kOwnership),4,2,0x1234,0,1024,0};
  Check(std::vector<uint32_t>(trace.words().begin(),trace.words().end())==expected,
        "record payloads and operation IDs must be exact");
  Check(!trace.CloseFrame(179) && trace.active(), "unrelated frame cannot close trace");
  Check(trace.CloseFrame(180) && !trace.active(), "capture closes on its own frame");
  trace.Record(Kind::kExtent,{99});
  Check(trace.words().size()==expected.size(), "post-frame records cannot leak");
  Check(!trace.BeginOperation(180,true), "finished frame must not be restarted");
  Check(!trace.BeginOperation(181,true), "intervening frames are not captured");
  Check(trace.BeginOperation(280,true), "second selected frame starts independently");
  Check(trace.header()[4]==1 && trace.header()[5]==1 && trace.words().size()==3,
        "second capture must reset operation IDs, data and drop counts");
  Check(trace.CloseFrame(280), "second frame closes");
  Check(!trace.BeginOperation(281,true), "no continuous tracing after second frame");

  Recorder small(10);
  Check(small.BeginOperation(180,true),"small capture begins");
  small.Record(Kind::kExtent,{1,2,3,4}); // exactly fills the ten-word capacity.
  Check(small.words().size()==10,"exact-capacity record fits");
  small.Record(Kind::kOwnership,{5});
  Check(small.words().size()==10 && small.header()[6]==1,"overflow cannot truncate a record");
  small.Record(Kind::kExtent,{});
  Check(small.words().size()==10 && small.header()[6]==2,"overflow must preserve a contiguous prefix");
  Check(small.CloseFrame(180),"overflow still permits explicit frame closure");
  Check(small.header()[5]==2 && small.header()[7]==10,"header counts only complete records");

  Recorder fixture;
  fixture.BeginOperation(180,true);
  fixture.Record(Kind::kExtent,{8192,480,1});
  fixture.Record(Kind::kVertexExport,{62,15,0x3f800000,0,0x3f800000,0x3f800000});
  fixture.CloseFrame(180);
  auto header=fixture.header();
  FILE* file=std::fopen("fixture.bin","wb"); Check(file!=nullptr,"fixture file opens");
  Check(std::fwrite(header.data(),4,header.size(),file)==header.size(),"fixture header writes");
  Check(std::fwrite(fixture.words().data(),4,fixture.words().size(),file)==fixture.words().size(),"fixture payload writes");
  Check(std::fclose(file)==0,"fixture file closes");
  std::puts("PASS: disabled capture, causal IDs, exact records, bounded overflow, closure and separated frames");
}
