#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>

namespace lb2::depth_clear {
inline constexpr std::array<uint32_t,27> kProgram={
  0x30052003,0x1200,0xc4000000,0x1005,0x1200,0xc2000000,0x1006,0x10071200,0x22000000,
  0x30081000,0x393a88,0x7,0x80000,0x40263688,0x300,0xc80f8000,0,0xe2000000,
  0xc80f803e,0,0xe2010100,0xc8000000,0,0xe2000000,0,0,0};
inline bool ProgramMatches(const uint32_t* code,size_t count) {
  return code && count==kProgram.size() && std::equal(kProgram.begin(),kProgram.end(),code);
}
struct State {
  bool program_matches=false,has_ps=false,memexport=false,query=false,rasterized=false;
  bool half_pixel_offset=false,rectangle_draw=false;
  bool allow_depth_only_alpha=false;
  bool allow_stencil_partial=false;
  uint32_t edram_mode=0;
  uint32_t depth_control=0,color_mask=0,stencil_front=0,stencil_back=0;
  uint32_t draw_initiator=0,clip_control=0,vte_control=0,sc_mode=0,vertex_control=0;
  uint32_t aa_mask=0,window_offset=0,window_tl=0,screen_tl=0,window_br=0,screen_br=0;
  uint32_t program_control=0,color_control=0,cliprect_rule=0,visibility_query=0;
};
struct Context {
  bool eligible=false,bad=false,pixel_zero=false,deferred=false,committed=false,depth_only_alpha=false;
  bool stencil_partial=false,stencil_replaying=false,stencil_ref_one=false;
  unsigned vertices=0,current=0,exported=0;
  uint32_t height=0,used_rts=0;
  std::array<std::array<float,4>,3> positions{};
  void (*restore)(void*)=nullptr;
  void* restore_context=nullptr;
  bool (*begin_replay)(void*)=nullptr;
  void* begin_context=nullptr;
};
class Scope {
 public:
  Scope(const State& state,bool enabled,bool (*begin_replay)(void*)=nullptr,void* begin_context=nullptr);
  ~Scope();
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
 private:
  Context context_;
  Context* previous_;
};
}
extern "C" void Lb2DepthClearResetVertices();
extern "C" void Lb2DepthClearVertex(unsigned index);
extern "C" void Lb2DepthClearPosition(uint32_t mask,const float* position);
extern "C" void Lb2DepthClearHeight(uint32_t height,uint32_t used_rts);
extern "C" bool Lb2DepthClearCutout(uint32_t key,uint32_t height,uint32_t scale_x,uint32_t scale_y);
extern "C" bool Lb2DepthClearDefer(void (*restore)(void*),void* context);
extern "C" void Lb2DepthClearDrawEmitted(bool real_pipeline=true);
extern "C" void Lb2DepthClearReport();
extern "C" bool Lb2StencilClearCutout(uint32_t key,uint32_t height,uint32_t scale_x,uint32_t scale_y);
extern "C" bool Lb2StencilClearDefer(void (*restore)(void*),void* context);
// 0: original transfer, 1: preserve depth/full nativezero and crop stencil bits,
// 2: fallback full stencil-only replay, with no depth/host-depth store.
extern "C" unsigned Lb2StencilClearTransferMode(uint32_t key);
