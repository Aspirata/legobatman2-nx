#include <cstdio>
#include <cstdlib>
#include "../src/platform/switch/lb2_stencil_zero.h"
#include "../src/platform/switch/lb2_depth_clear.h"
void Check(bool value,const char* why){if(!value){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
lb2::stencil_zero::TransferPlan ValidPlan(){
  lb2::stencil_zero::TransferPlan p;
  p.source_image=100;p.source_key=0x003062d0;p.dest_key=0x002062d0;
  p.start_tiles=720;p.end_tiles=1440;p.rectangle_count=1;p.width=960;p.height=960;
  p.scale_x=p.scale_y=1;p.single_depth_target=true;
  return p;
}
void FullClear(bool real=true,bool enabled=true,bool destroy_before_emit=false){
  lb2::stencil_zero::Scope zero(enabled);
  lb2::depth_clear::State s;
  s.program_matches=s.rasterized=s.half_pixel_offset=s.rectangle_draw=true;
  s.depth_control=0x8777;s.stencil_front=0x00ff0000;s.stencil_back=0x00ffff00;
  s.draw_initiator=0x30088;s.clip_control=0x10000;s.vte_control=0x300;
  s.sc_mode=0x10000;s.vertex_control=5;s.aa_mask=0xffff;
  s.window_br=s.screen_br=0x20002000;s.program_control=0x10010001;
  lb2::depth_clear::Scope clear(s,true);
  Lb2StencilZeroBeginDraw();
  const float positions[3][4]={{0,0,1,1},{480,0,1,1},{480,480,1,1}};
  for(unsigned i=0;i<3;++i){Lb2DepthClearVertex(i);Lb2DepthClearPosition(15,positions[i]);}
  Lb2DepthClearHeight(480,1);
  Check(Lb2DepthClearCutout(0x003062d0,480,1,1),"producer uses the actual full clear proof");
  Lb2StencilZeroBindTarget(100);
  if(destroy_before_emit)Lb2StencilZeroForget(100);
  Lb2DepthClearDrawEmitted(real);
}
int main(){
  using namespace lb2::stencil_zero;const auto plan=ValidPlan();
  {Scope scope(true);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"unproven source cannot omit stencil");}
  FullClear(false);
  {Scope scope(true);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"skipped/placeholder clear cannot publish zero");}
  FullClear(true,false);
  {Scope scope(true);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"disabled trial cannot publish zero");}
  FullClear();
  {Scope scope(false);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"disabled consumer preserves imports");}
  FullClear();
  {Scope scope(true);Lb2StencilZeroBeginDraw();auto p=plan;p.source_image=101;
    Check(!TryReuse(p),"different physical image cannot borrow proof");
    Check(TryReuse(plan),"next material draw reuses witnessed zero generation");
    Check(!TryReuse(plan),"proof consumed at most once");}
  FullClear();{Scope unrelated(true);Lb2StencilZeroBeginDraw();}
  {Scope scope(true);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"unrelated material draw expires proof");}
  FullClear();Lb2StencilZeroInvalidate();
  {Scope scope(true);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"resolve expires prior generation");}
  FullClear();Lb2StencilZeroForget(100);
  {Scope scope(true);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"destroyed/reused image handle cannot inherit proof");}
  FullClear(true,true,true);
  {Scope scope(true);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"destruction cancels a pending producer publication");}
  {Scope scope(true);lb2::depth_clear::State invalid;lb2::depth_clear::Scope clear(invalid,true);
    Lb2StencilZeroBeginDraw();Lb2StencilZeroBindTarget(100);Lb2DepthClearDrawEmitted(true);}
  {Scope scope(true);Lb2StencilZeroBeginDraw();Check(!TryReuse(plan),"binding a target without full clear proof cannot publish zero");}
  FullClear();
  {Scope scope(true);Lb2StencilZeroBeginDraw();Lb2StencilZeroForget(100);
    Check(!TryReuse(plan),"destruction also invalidates current draw's proof");}
  FullClear();
  {Scope scope(true);Lb2StencilZeroBeginDraw();
    uint32_t TransferPlan::* fields[]={&TransferPlan::source_key,&TransferPlan::dest_key,
      &TransferPlan::start_tiles,&TransferPlan::end_tiles,&TransferPlan::rectangle_count,
      &TransferPlan::x,&TransferPlan::y,&TransferPlan::width,&TransferPlan::height,
      &TransferPlan::scale_x,&TransferPlan::scale_y};
    for(auto field:fields){auto p=plan;p.*field^=1;Check(!TryReuse(p),"changed transfer geometry/key/range rejected");}
    {auto p=plan;p.single_depth_target=false;Check(!TryReuse(p),"other targets rejected");}
    {auto p=plan;p.stencil_export=true;Check(!TryReuse(p),"combined stencil-export path rejected");}
    {auto p=plan;p.resolve_or_cutout=true;Check(!TryReuse(p),"resolve/partial import rejected");}
    {auto p=plan;p.host_depth_source=true;Check(!TryReuse(p),"host-depth alias path rejected");}
    Check(TryReuse(plan),"rejected plans do not consume eligible proof");
  }
  FullClear();
  {Scope scope(true);Lb2StencilZeroBeginDraw();Lb2StencilZeroInvalidate();Check(!TryReuse(plan),"resolve also expires current proof");}
  std::puts("PASS: real full-clear zero proof, one material consumer, physical lifetime and conservative transfer guards");
}
