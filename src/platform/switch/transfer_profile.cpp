#include "lb2_gpu_profile.h"
#include "lb2_fence_samples.h"
#include <rex/logging.h>
#include <algorithm>
#include <array>
#include <mutex>

namespace {
struct Group {
  uint32_t shader=0, source=0, dest=0;
  unsigned kind=0;
  uint64_t draws=0, rectangles=0, pixels=0;
};
struct Snapshot {
  std::array<Group,96> groups{};
  uint64_t draws[3]{}, rectangles=0, pixels=0, ungrouped=0;
  uint64_t fast_groups=0,fast_rectangles=0,fast_bytes=0;
  unsigned used=0;
};
Snapshot state;
std::mutex transfer_mutex;
}

extern "C" void Lb2GpuProfileTransfer(uint32_t shader,uint32_t source,uint32_t dest,
                                     unsigned kind,uint32_t rectangles,uint64_t pixels) {
  if(kind>=3)return;
  Lb2FenceSampleTransfer(pixels);
  std::lock_guard<std::mutex> lock(transfer_mutex);
  ++state.draws[kind];state.rectangles+=rectangles;state.pixels+=pixels;
  unsigned i=0;
  for(;i<state.used;++i){
    const auto& g=state.groups[i];
    if(g.shader==shader&&g.source==source&&g.dest==dest&&g.kind==kind)break;
  }
  if(i==state.used){
    if(state.used==state.groups.size()){++state.ungrouped;return;}
    state.groups[state.used++]={shader,source,dest,kind};
  }
  auto& group=state.groups[i];++group.draws;group.rectangles+=rectangles;group.pixels+=pixels;
}

extern "C" void Lb2GpuProfileFastStencilCopy(uint32_t rectangles,uint64_t bytes) {
  std::lock_guard<std::mutex> lock(transfer_mutex);
  ++state.fast_groups;state.fast_rectangles+=rectangles;state.fast_bytes+=bytes;
}

extern "C" void Lb2GpuProfileTransfersReport() {
  Snapshot report;
  {
    std::lock_guard<std::mutex> lock(transfer_mutex);
    report=state;state={};
  }
  REXLOG_INFO("LB2 transfers: color={} depth={} stencil_bit={} rectangles={} recorded_pixels={} "
              "ungrouped_draws={} fast_stencil_groups={} fast_rectangles={} fast_bytes={}; "
              "top retained groups by draw count; area is not measured GPU cost",
              report.draws[0],report.draws[1],report.draws[2],report.rectangles,report.pixels,report.ungrouped,
              report.fast_groups,report.fast_rectangles,report.fast_bytes);
  std::sort(report.groups.begin(),report.groups.begin()+report.used,
            [](const Group& a,const Group& b){return a.draws>b.draws;});
  constexpr const char* kinds[]={"color","depth","stencil_bit"};
  for(unsigned i=0;i<std::min(report.used,16u);++i){
    const auto& g=report.groups[i];
    REXLOG_INFO("LB2 transfer group: kind={} shader={:08X} source={:08X} dest={:08X} "
                "draws={} rectangles={} recorded_pixels={}",
                kinds[g.kind],g.shader,g.source,g.dest,g.draws,g.rectangles,g.pixels);
  }
}
