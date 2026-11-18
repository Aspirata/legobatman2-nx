#include "lb2_clear_trace.h"
#include <rex/logging.h>
#include <algorithm>
#include <cstdio>

namespace {
using lb2::clear_trace::Kind;
lb2::clear_trace::Recorder recorder;
std::array<uint32_t,1024> previous_regs{};
std::array<uint32_t,2048> changes{};
uint64_t previous_frame=0;
bool clear_vertices=false;
}
extern "C" bool Lb2ClearTraceActive() { return recorder.active(); }
extern "C" bool Lb2ClearTraceVertices() { return recorder.active() && clear_vertices; }
extern "C" void Lb2ClearTraceRecord(unsigned kind,const uint32_t* words,uint32_t count) {
  recorder.Record(Kind(kind),std::span<const uint32_t>(words,count));
}
extern "C" void Lb2ClearTraceTextureBindings(uint32_t mask,const uint32_t* regs) {
  if (!recorder.active()) return;
  std::array<uint32_t,225> payload;
  const size_t size=lb2::clear_trace::PackTextureBindings(mask,regs+0x4800,payload);
  recorder.Record(Kind::kTextureBindings,std::span<const uint32_t>(payload.data(),size));
}
extern "C" void Lb2ClearTraceBegin(uint64_t frame,bool enabled,const uint32_t* regs) {
  clear_vertices=false;
  if (!recorder.BeginOperation(frame,enabled)) return;
  const bool first=previous_frame!=frame;
  uint32_t count=0;
  // Full raster/viewport/draw/resolve register range first, then exact deltas.
  for (unsigned i=0;i<previous_regs.size();++i) {
    const uint32_t value=regs[0x2000+i];
    if (first || value!=previous_regs[i]) { changes[count++]=0x2000+i; changes[count++]=value; }
    previous_regs[i]=value;
  }
  previous_frame=frame;
  recorder.Record(Kind::kRegisters,std::span<const uint32_t>(changes.data(),count));
}
extern "C" void Lb2ClearTraceShader(unsigned stage,uint64_t hash,const uint32_t* code,uint32_t count,const uint32_t* regs) {
  if (!recorder.active()) return;
  recorder.Record(Kind::kShader,{stage,uint32_t(hash),uint32_t(hash>>32),count});
  if (stage==0) clear_vertices=hash==UINT64_C(0x1E6883FCCDE1F688);
  // Preserve exact clear programs and constants for offline interpretation.
  if (clear_vertices) {
    recorder.Record(Kind::kShaderCode,std::span<const uint32_t>(code,count));
    if (stage==0) recorder.Record(Kind::kConstants,std::span<const uint32_t>(regs+0x4000,0x928));
  }
}
extern "C" void Lb2ClearTraceFinish(uint64_t frame) {
  if (!recorder.CloseFrame(frame)) return;
  clear_vertices=false;
  char path[128];
  std::snprintf(path,sizeof(path),"/switch/legobatman2/logs/lb2-clear-trace-%llu.bin",(unsigned long long)frame);
  const auto header=recorder.header();
  const auto words=recorder.words();
  FILE* file=std::fopen(path,"wb");
  bool ok=false;
  if (file) {
    ok=std::fwrite(header.data(),4,header.size(),file)==header.size();
    if (ok) ok=std::fwrite(words.data(),4,words.size(),file)==words.size();
    if (std::fclose(file)!=0) ok=false;
  }
  REXLOG_INFO("LB2 clear trace: frame={} saved={} operations={} records={} dropped={} bytes={} path={}; CPU causal trace, no GPU timings or queue drains",
              frame,ok,header[4],header[5],header[6],(header.size()+words.size())*4,path);
}
