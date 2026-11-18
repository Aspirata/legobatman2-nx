#include "lb2_depth_clear.h"
#include "lb2_stencil_zero.h"
#include <rex/logging.h>
#include <atomic>
#include <cmath>

namespace {
using namespace lb2::depth_clear;
thread_local Context* current=nullptr;
std::atomic<uint64_t> eligible{0},deferred{0},committed{0},restored{0},replay_failed{0};
std::atomic<uint64_t> alpha_committed{0};
std::atomic<uint64_t> stencil_eligible{0},stencil_deferred{0},stencil_committed{0};
std::atomic<uint64_t> stencil_restored{0},stencil_failed{0},stencil_ref_one{0};
bool Eligible(const State& s) {
  // In this exact DepthOnly/D24Unorm hostRT case the normal pipeline has no
  // fragment stage, so the leftover guest alpha-test register is not executed.
  const bool alpha_safe=!(s.color_control & 0x18) ||
    (s.allow_depth_only_alpha && s.edram_mode==5 &&
     (s.color_control==0x8700000e || s.color_control==0x8700000c));
  return s.program_matches && !s.has_ps && !s.memexport && !s.query && s.rasterized && s.half_pixel_offset && s.rectangle_draw &&
    s.depth_control==0x8777 && !s.color_mask && s.stencil_front==0x00ff0000 && s.stencil_back==0x00ffff00 &&
    s.draw_initiator==0x30088 && s.clip_control==0x10000 && s.vte_control==0x300 &&
    s.sc_mode==0x10000 && (s.vertex_control & ~uint32_t(1))==4 && s.aa_mask==0xffff &&
    !s.window_offset && !s.window_tl && !s.screen_tl &&
    s.window_br==0x20002000 && s.screen_br==0x20002000 && s.program_control==0x10010001 &&
    alpha_safe && !s.cliprect_rule && !s.visibility_query;
}
bool StencilEligible(const State& s) {
  return s.program_matches && !s.has_ps && !s.memexport && !s.query && s.rasterized &&
    s.half_pixel_offset && s.rectangle_draw && s.edram_mode==5 &&
    s.depth_control==0x8701 && !s.color_mask &&
    (s.stencil_front==0x00ff0000 || s.stencil_front==0x00ff0001) && s.stencil_back==0x00ffff00 &&
    s.draw_initiator==0x30088 && s.clip_control==0x10000 && s.vte_control==0x300 &&
    s.sc_mode==0x10000 && s.vertex_control==5 && s.aa_mask==0xffff &&
    !s.window_offset && !s.window_tl && !s.screen_tl &&
    s.window_br==0x20002000 && s.screen_br==0x20002000 && s.program_control==0x10010001 &&
    (s.color_control==0x87000004 || s.color_control==0x87000006) &&
    !s.cliprect_rule && !s.visibility_query;
}
}
namespace lb2::depth_clear {
Scope::Scope(const State& state,bool enabled,bool (*begin_replay)(void*),void* begin_context) : previous_(current) {
  context_.stencil_partial=state.allow_stencil_partial && StencilEligible(state);
  context_.eligible=(enabled && Eligible(state)) || context_.stencil_partial;
  context_.pixel_zero=!(state.vertex_control & 1);
  context_.depth_only_alpha=bool(state.color_control & 8);
  context_.stencil_ref_one=(state.stencil_front & 0xff)==1;
  context_.begin_replay=begin_replay; context_.begin_context=begin_context;
  if (context_.stencil_partial) stencil_eligible.fetch_add(1,std::memory_order_relaxed);
  else if (context_.eligible) eligible.fetch_add(1,std::memory_order_relaxed);
  current=&context_;
}
Scope::~Scope() {
  if (context_.deferred && !context_.committed) {
    context_.deferred=false;
    context_.stencil_replaying=context_.stencil_partial;
    if (!context_.begin_replay || context_.begin_replay(context_.begin_context)) {
      context_.restore(context_.restore_context);
      (context_.stencil_partial?stencil_restored:restored).fetch_add(1,std::memory_order_relaxed);
    } else {
      (context_.stencil_partial?stencil_failed:replay_failed).fetch_add(1,std::memory_order_relaxed);
      REXLOG_ERROR("LB2 depth clear fallback could not begin submission");
    }
    context_.stencil_replaying=false;
  }
  current=previous_;
}
}
extern "C" void Lb2DepthClearResetVertices() {
  if (!current || !current->eligible) return;
  current->vertices=current->current=current->exported=0;
  current->height=current->used_rts=0;
  current->bad=false;
}
extern "C" void Lb2DepthClearVertex(unsigned index) {
  if (!current || !current->eligible) return;
  if (index>=3 || index!=current->vertices) { current->bad=true; return; }
  current->current=index; ++current->vertices;
}
extern "C" void Lb2DepthClearPosition(uint32_t mask,const float* position) {
  if (!current || !current->eligible) return;
  if (mask!=15 || !current->vertices || current->bad ||
      (current->exported & (1u<<current->current))) { current->bad=true; return; }
  std::copy_n(position,4,current->positions[current->current].begin());
  current->exported|=1u<<current->current;
}
extern "C" bool Lb2DepthClearCutout(uint32_t key,uint32_t height,uint32_t scale_x,uint32_t scale_y) {
  if (!current || !current->eligible || current->stencil_partial || current->bad || current->vertices!=3 || current->exported!=7 ||
      key!=0x003062d0 || height!=480 || current->height!=480 || current->used_rts!=1 || scale_x!=1 || scale_y!=1) return false;
  const float offset=current->pixel_zero ? 0.5f : 0.0f;
  constexpr float corners[3][2]={{0,0},{480,0},{480,480}};
  for (unsigned i=0;i<3;++i) {
    const auto& p=current->positions[i];
    for (float v:p) if (!std::isfinite(v)) return false;
    if (p[0]+offset!=corners[i][0] || p[1]+offset!=corners[i][1] || p[2]!=1 || p[3]!=1) return false;
  }
  return true;
}
extern "C" void Lb2DepthClearHeight(uint32_t height,uint32_t used_rts) {
  if (!current || !current->eligible) return;
  current->height=height; current->used_rts=used_rts;
}
extern "C" bool Lb2DepthClearDefer(void (*restore)(void*),void* context) {
  if (!restore || !current || current->deferred || !Lb2DepthClearCutout(0x003062d0,480,1,1)) return false;
  current->restore=restore; current->restore_context=context; current->deferred=true;
  deferred.fetch_add(1,std::memory_order_relaxed); return true;
}
extern "C" void Lb2DepthClearDrawEmitted(bool real_pipeline) {
  if(real_pipeline && Lb2DepthClearCutout(0x003062d0,480,1,1))Lb2StencilZeroCommit();
  if (!real_pipeline || !current || !current->deferred || current->committed) return;
  current->committed=true;
  if(current->stencil_partial){
    stencil_committed.fetch_add(1,std::memory_order_relaxed);
    if(current->stencil_ref_one)stencil_ref_one.fetch_add(1,std::memory_order_relaxed);
    return;
  }
  committed.fetch_add(1,std::memory_order_relaxed);
  if (current->depth_only_alpha) alpha_committed.fetch_add(1,std::memory_order_relaxed);
}
extern "C" void Lb2DepthClearReport() {
  Lb2StencilZeroReport();
  REXLOG_INFO("LB2 depth clear cutout: eligible={} deferred={} committed={} replay_attempts={} replay_failed={} alpha_committed={}; only full480x480 4x depth+stencil, original guest draw retained",
    eligible.exchange(0,std::memory_order_relaxed),deferred.exchange(0,std::memory_order_relaxed),
    committed.exchange(0,std::memory_order_relaxed),restored.exchange(0,std::memory_order_relaxed),
    replay_failed.exchange(0,std::memory_order_relaxed),alpha_committed.exchange(0,std::memory_order_relaxed));
  REXLOG_INFO("LB2 stencil clear cutout: eligible={} deferred={} committed={} replay_attempts={} replay_failed={} ref_one_committed={}; only400x240 stencil overwrite, depth and40x240 border preserved",
    stencil_eligible.exchange(0,std::memory_order_relaxed),stencil_deferred.exchange(0,std::memory_order_relaxed),
    stencil_committed.exchange(0,std::memory_order_relaxed),stencil_restored.exchange(0,std::memory_order_relaxed),
    stencil_failed.exchange(0,std::memory_order_relaxed),stencil_ref_one.exchange(0,std::memory_order_relaxed));
}
extern "C" bool Lb2StencilClearCutout(uint32_t key,uint32_t height,uint32_t scale_x,uint32_t scale_y) {
  if(!current || !current->eligible || !current->stencil_partial || current->bad ||
     current->vertices!=3 || current->exported!=7 || key!=0x00305eb6 || height!=240 ||
     current->height!=240 || current->used_rts!=1 || scale_x!=1 || scale_y!=1)return false;
  constexpr float corners[3][2]={{0,0},{400,0},{400,240}};
  for(unsigned i=0;i<3;++i){
    const auto& p=current->positions[i];
    for(float v:p)if(!std::isfinite(v))return false;
    if(p[0]!=corners[i][0] || p[1]!=corners[i][1] || p[2]!=0 || p[3]!=1)return false;
  }
  return true;
}
extern "C" bool Lb2StencilClearDefer(void (*restore)(void*),void* context) {
  if(!restore || !current || current->deferred || !Lb2StencilClearCutout(0x00305eb6,240,1,1))return false;
  current->restore=restore;current->restore_context=context;current->deferred=true;
  stencil_deferred.fetch_add(1,std::memory_order_relaxed);
  return true;
}
extern "C" unsigned Lb2StencilClearTransferMode(uint32_t key) {
  if(!current || !current->stencil_partial || key!=0x00305eb6)return 0;
  if(current->stencil_replaying)return 2;
  return current->deferred?1:0;
}
