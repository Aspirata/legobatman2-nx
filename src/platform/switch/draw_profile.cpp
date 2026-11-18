#include "lb2_draw_profile.h"
#include "lb2_fence_samples.h"
#include <rex/logging.h>
#include <algorithm>
#include <array>
#include <mutex>
namespace {
constexpr unsigned capacity=512;
struct Group {
  Lb2GuestDrawKey key{};
  uint64_t draws=0,vertices=0,pixels=0;
};
struct Snapshot {
  std::array<Group,capacity> groups{};
  uint64_t draws=0,vertices=0,pixels=0,memexport=0,rasterized=0,ungrouped=0;
  unsigned used=0;
};
Snapshot state,report; // Bounded static storage, not the presentation thread's stack.
std::mutex draw_mutex,report_mutex;
uint64_t Hash(const Lb2GuestDrawKey& k) {
  uint64_t h=0;
  auto add=[&](uint64_t v){h^=v+0x9e3779b97f4a7c15ull+(h<<6)+(h>>2);};
  add(k.vs);add(k.ps);add(k.surface);add(k.depth);add(k.color0);
  add(k.depth_control);add(k.color_mask);add(k.flags);add(k.primitive);
  add(uint32_t(k.x));add(uint32_t(k.y));add(k.width);add(k.height);
  return h;
}
}
extern "C" void Lb2GpuProfileDraw(const Lb2GuestDrawKey* key,uint32_t vertices) {
  if(!key)return;
  Lb2FenceSampleGuestDraw(vertices);
  const uint64_t pixels=uint64_t(key->width)*key->height;
  const unsigned start=unsigned(Hash(*key))&(capacity-1);
  std::lock_guard<std::mutex> lock(draw_mutex);
  ++state.draws;state.vertices+=vertices;state.pixels+=pixels;
  state.memexport+=(key->flags&1)!=0;state.rasterized+=(key->flags&2)!=0;
  for(unsigned probe=0;probe<capacity;++probe){
    auto& g=state.groups[(start+probe)&(capacity-1)];
    if(!g.draws){g.key=*key;++state.used;}
    else if(!(g.key==*key))continue;
    ++g.draws;g.vertices+=vertices;g.pixels+=pixels;return;
  }
  ++state.ungrouped;
}
extern "C" void Lb2GpuProfileDrawsReport() {
  std::lock_guard<std::mutex> serialize_report(report_mutex);
  {
    std::lock_guard<std::mutex> lock(draw_mutex);
    report=state;state={};
  }
  REXLOG_INFO("LB2 guest draws: draws={} vertices_or_indices={} scissor_pixels={} "
              "memexport_draws={} rasterized_draws={} ungrouped_draws={} groups={}; "
              "workload proxies only, no measured GPU time",
              report.draws,report.vertices,report.pixels,report.memexport,
              report.rasterized,report.ungrouped,report.used);
  std::array<unsigned,capacity> order{};
  std::array<bool,capacity> emitted{};
  unsigned count=0;
  for(unsigned i=0;i<capacity;++i)if(report.groups[i].draws)order[count++]=i;
  for(unsigned ranking=0;ranking<2;++ranking){
    std::sort(order.begin(),order.begin()+count,[&](unsigned a,unsigned b){
      const auto& x=report.groups[a];const auto& y=report.groups[b];
      const uint64_t xv=ranking?x.pixels:x.draws,yv=ranking?y.pixels:y.draws;
      return xv!=yv?xv>yv:a<b;
    });
    for(unsigned j=0;j<std::min(count,12u);++j){
      const unsigned i=order[j];if(emitted[i])continue;emitted[i]=true;
      const auto& g=report.groups[i];const auto& k=g.key;
      REXLOG_INFO("LB2 guest draw group: slot={} vs={:016X} ps={:016X} "
                  "surface={:08X} depth={:08X} color0={:08X} depth_control={:08X} "
                  "color_mask={:08X} flags={} primitive={} scissor={},{},{}x{} "
                  "draws={} vertices_or_indices={} scissor_pixels={}",
                  i,k.vs,k.ps,k.surface,k.depth,k.color0,k.depth_control,k.color_mask,
                  k.flags,k.primitive,k.x,k.y,k.width,k.height,g.draws,g.vertices,g.pixels);
    }
  }
}
