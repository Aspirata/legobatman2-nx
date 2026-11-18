#include "lb2_stencil_zero.h"
#include <rex/logging.h>
#include <atomic>

namespace {
using namespace lb2::stencil_zero;
thread_local Context* current=nullptr;
// Physical image proof, valid only until the next material draw. All image
// lifecycle calls run on the CP thread or after it has stopped at shutdown.
uint64_t zero_image=0;
std::atomic<uint64_t> published{0},reused{0},avoided{0};
}
namespace lb2::stencil_zero {
Scope::Scope(bool enabled):previous_(current){context_.enabled=enabled;current=&context_;}
Scope::~Scope(){current=previous_;}
bool TryReuse(const TransferPlan& p){
  if(!current || !current->enabled || !p.source_image || p.source_image!=current->source_image ||
     p.source_key!=0x003062d0 || p.dest_key!=0x002062d0 || p.start_tiles!=720 || p.end_tiles!=1440 ||
     p.rectangle_count!=1 || p.x || p.y || p.width!=960 || p.height!=960 ||
     p.scale_x!=1 || p.scale_y!=1 || !p.single_depth_target || p.stencil_export ||
     p.resolve_or_cutout || p.host_depth_source)return false;
  current->source_image=0;
  reused.fetch_add(1,std::memory_order_relaxed);
  return true;
}
}
extern "C" void Lb2StencilZeroBeginDraw(){
  if(current){
    current->source_image=current->enabled?zero_image:0;
    current->target_image=0;current->published=false;
  }
  zero_image=0;
}
extern "C" void Lb2StencilZeroBindTarget(uint64_t image){
  if(current && current->enabled)current->target_image=image;
}
extern "C" void Lb2StencilZeroCommit(){
  if(!current || !current->enabled || !current->target_image || current->published)return;
  zero_image=current->target_image;current->published=true;
  published.fetch_add(1,std::memory_order_relaxed);
}
extern "C" void Lb2StencilZeroInvalidate(){
  zero_image=0;
  if(current)current->source_image=current->target_image=0;
}
extern "C" void Lb2StencilZeroForget(uint64_t image){
  if(zero_image==image)zero_image=0;
  if(current){
    if(current->source_image==image)current->source_image=0;
    if(current->target_image==image)current->target_image=0;
  }
}
extern "C" void Lb2StencilZeroSkipped(unsigned draws){avoided.fetch_add(draws,std::memory_order_relaxed);}
extern "C" void Lb2StencilZeroReport(){
  REXLOG_INFO("LB2 stencil zero reuse: published={} groups={} avoided_draws={}; exact full-clear generation, one material consumer, original depth/nativezero retained",
    published.exchange(0,std::memory_order_relaxed),reused.exchange(0,std::memory_order_relaxed),
    avoided.exchange(0,std::memory_order_relaxed));
}
