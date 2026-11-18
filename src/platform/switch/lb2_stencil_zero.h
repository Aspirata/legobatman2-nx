#pragma once
#include <cstdint>

namespace lb2::stencil_zero {
struct TransferPlan {
  uint64_t source_image=0;
  uint32_t source_key=0,dest_key=0,start_tiles=0,end_tiles=0;
  uint32_t rectangle_count=0,x=0,y=0,width=0,height=0,scale_x=0,scale_y=0;
  bool single_depth_target=false,stencil_export=false,resolve_or_cutout=false,host_depth_source=false;
};
struct Context {
  bool enabled=false,published=false;
  uint64_t source_image=0,target_image=0;
};
// One GPU command processor thread, nested scopes restore their caller.
class Scope {
 public:
  explicit Scope(bool enabled);
  ~Scope();
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
 private:
  Context context_;
  Context* previous_;
};
bool TryReuse(const TransferPlan& plan);
}
extern "C" void Lb2StencilZeroBeginDraw();
extern "C" void Lb2StencilZeroBindTarget(uint64_t image);
// Called only after the existing exact full clear proof and real guest draw.
extern "C" void Lb2StencilZeroCommit();
extern "C" void Lb2StencilZeroInvalidate();
extern "C" void Lb2StencilZeroForget(uint64_t image);
extern "C" void Lb2StencilZeroSkipped(unsigned draws);
extern "C" void Lb2StencilZeroReport();
