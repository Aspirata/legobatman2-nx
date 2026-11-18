#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "../src/platform/switch/lb2_depth_clear.h"

void Check(bool ok,const char* why) {
  if (!ok) { std::fprintf(stderr,"FAIL: %s\n",why); std::exit(1); }
}
lb2::depth_clear::State Valid() {
  lb2::depth_clear::State s;
  s.program_matches=true; s.rasterized=true; s.half_pixel_offset=true; s.rectangle_draw=true;
  s.depth_control=0x8777; s.stencil_front=0x00ff0000; s.stencil_back=0x00ffff00;
  s.draw_initiator=0x30088; s.clip_control=0x10000; s.vte_control=0x300;
  s.sc_mode=0x10000; s.vertex_control=5; s.aa_mask=0xffff;
  s.window_br=s.screen_br=0x20002000; s.program_control=0x10010001;
  return s;
}
void Vertices(float offset=0) {
  const float positions[3][4]={{offset,offset,1,1},{480+offset,offset,1,1},{480+offset,480+offset,1,1}};
  for (unsigned i=0;i<3;++i) { Lb2DepthClearVertex(i); Lb2DepthClearPosition(15,positions[i]); }
  Lb2DepthClearHeight(480,1);
}
bool Cutout() { return Lb2DepthClearCutout(0x003062d0,480,1,1); }
int main() {
  using namespace lb2::depth_clear;
  const auto state=Valid();
  { Scope scope(state,false); Vertices(); Check(!Cutout(),"disabled trial preserves imports"); }
  { Scope scope(state,true); Check(!Cutout(),"missing interpreter execution preserves imports");
    Vertices(); Check(Cutout(),"exact witnessed4x480x480 depth/stencil overwrite admitted");
    Check(!Lb2DepthClearCutout(0x002062d0,480,1,1),"1x target rejected");
    Check(!Lb2DepthClearCutout(0x003062d0,479,1,1),"short ownership height rejected");
    Check(!Lb2DepthClearCutout(0x003062d0,480,2,1),"resolution scale rejected");
    { Scope nested(state,false); Vertices(); Check(!Cutout(),"nested disabled draw cannot use parent proof"); }
    Check(Cutout(),"nested context restores live parent proof");
  }
  Check(!Cutout(),"destroyed draw proof cannot leak to next operation");
  { auto s=state; s.vertex_control=4; Scope scope(s,true); Vertices(-0.5f);
    Check(Cutout(),"D3DZero half-pixel shift admits the same physical rectangle"); }
  { Scope scope(state,true); Vertices(-0.5f); Check(!Cutout(),"half-pixel mismatch preserves edge samples"); }
  { Scope scope(state,true); Vertices(); float v[4]={0,0,1,1};
    Lb2DepthClearVertex(3); Lb2DepthClearPosition(15,v); Check(!Cutout(),"extra vertices invalidate proof"); }
  for (unsigned component=0;component<4;++component) {
    Scope scope(state,true); Vertices(); float v[4]={480,480,1,1};
    v[component]=std::numeric_limits<float>::quiet_NaN();
    Lb2DepthClearPosition(15,v); Check(!Cutout(),"NaN or repeated export invalidates proof");
  }
  { Scope scope(state,true); float v[4]={0,0,1,1}; Lb2DepthClearVertex(0);
    Lb2DepthClearPosition(3,v); Vertices(); Check(!Cutout(),"partial or repeated position exports rejected"); }
  auto reject=[&](State s,const char* why) { Scope scope(s,true); Vertices(); Check(!Cutout(),why); };
  {auto s=state;s.program_matches=false;reject(s,"unknown shader rejected");}
  {auto s=state;s.has_ps=true;reject(s,"pixel shader rejected");}
  {auto s=state;s.memexport=true;reject(s,"memexport rejected");}
  {auto s=state;s.query=true;reject(s,"active query rejected");}
  {auto s=state;s.rasterized=false;reject(s,"disabled rasterization rejected");}
  {auto s=state;s.rectangle_draw=false;reject(s,"non-native or indexed draw rejected");}
  {auto s=state;s.half_pixel_offset=false;reject(s,"unsupported global pixel-center mode rejected");}
  uint32_t State::* fields[]={&State::depth_control,&State::stencil_front,&State::stencil_back,
    &State::draw_initiator,&State::clip_control,&State::vte_control,&State::sc_mode,
    &State::aa_mask,&State::window_offset,&State::window_tl,&State::screen_tl,
    &State::window_br,&State::screen_br,&State::program_control,&State::cliprect_rule,&State::visibility_query};
  for (auto field:fields) for (unsigned bit=0;bit<32;++bit) {
    auto s=state; s.*field^=uint32_t(1)<<bit; reject(s,"each changed guarded state bit rejected");
  }
  {auto s=state;s.color_mask=1;reject(s,"color write rejected");}
  {auto s=state;s.color_control=8;reject(s,"alpha test rejected");}
  {auto s=state;s.color_control=16;reject(s,"alpha-to-coverage rejected");}
  auto depth_alpha=state; depth_alpha.vertex_control=4;
  depth_alpha.color_control=0x8700000e; depth_alpha.edram_mode=5;
  depth_alpha.allow_depth_only_alpha=true;
  { Scope scope(depth_alpha,true); Vertices(-0.5f);
    Check(Cutout(),"exact DepthOnly state with no fragment stage admits alpha-test bypass"); }
  { auto s=depth_alpha;s.color_control=0x8700000c; Scope scope(s,true);Vertices(-0.5f);
    Check(Cutout(),"second witnessed alpha function is also unused without a fragment stage"); }
  auto reject_alpha=[&](State s,const char* why) { Scope scope(s,true); Vertices(-0.5f); Check(!Cutout(),why); };
  {auto s=depth_alpha;s.allow_depth_only_alpha=false;reject_alpha(s,"extension OFF preserves043 alpha rejection");}
  for(uint32_t mode: {0u,4u,6u}) {auto s=depth_alpha;s.edram_mode=mode;reject_alpha(s,"other EDRAM modes rejected");}
  {auto s=depth_alpha;s.has_ps=true;reject_alpha(s,"fragment shader forbids bypass");}
  {auto s=depth_alpha;s.color_control|=0x10;reject_alpha(s,"alpha-to-coverage remains forbidden");}
  {auto s=depth_alpha;s.color_control^=1;reject_alpha(s,"unobserved alpha function remains forbidden");}
  {auto s=state;s.vertex_control=1;reject(s,"unsupported rounding mode rejected");}
  Check(ProgramMatches(kProgram.data(),kProgram.size()),"witnessed exact program matches");
  for(unsigned word=0;word<kProgram.size();++word) {
    auto program=kProgram; program[word]^=1;
    Check(!ProgramMatches(program.data(),program.size()),"every code word participates in matching");
  }
  Check(!ProgramMatches(kProgram.data(),kProgram.size()-1),"shortened program rejected");
  Check(!ProgramMatches(nullptr,0),"missing program rejected");
  unsigned restored=0;
  auto restore=[](void* p){++*static_cast<unsigned*>(p);};
  { Scope scope(state,true); Vertices(); Check(Cutout(),"proof before resetting estimator");
    Lb2DepthClearResetVertices(); Check(!Cutout(),"new estimator entry invalidates old positions"); }
  { Scope scope(state,true); Vertices();
    Check(Lb2DepthClearDefer(restore,&restored),"full transfer list can be deferred");
    Check(!Lb2DepthClearDefer(restore,&restored),"same draw cannot defer twice"); }
  Check(restored==1,"pipeline placeholder, binding failure or skipped draw restores imports on exit");
  { Scope scope(state,true); Vertices(); Lb2DepthClearDefer(restore,&restored);
    Lb2DepthClearDrawEmitted(); }
  Check(restored==1,"actually emitted overwrite consumes the deferred imports");
  { Scope scope(state,true,[](void*){return false;},nullptr); Vertices();
    Lb2DepthClearDefer(restore,&restored); }
  Check(restored==1,"failed command submission cannot falsely report a completed replay");
  { Scope scope(state,false); Vertices(); Check(!Lb2DepthClearDefer(restore,&restored),"disabled draw cannot defer"); }
  { Scope scope(state,true); Vertices(); Lb2DepthClearDefer(restore,&restored);
    Lb2DepthClearDrawEmitted(false); }
  Check(restored==2,"cached placeholder cannot commit even if async mode was changed");
  auto stencil=state;
  stencil.allow_stencil_partial=true;stencil.edram_mode=5;
  stencil.depth_control=0x8701;stencil.color_control=0x87000004;
  auto stencil_vertices=[](float width=400,float height=240,float z=0,float w=1) {
    const float p[3][4]={{0,0,z,w},{width,0,z,w},{width,height,z,w}};
    for(unsigned i=0;i<3;++i){Lb2DepthClearVertex(i);Lb2DepthClearPosition(15,p[i]);}
    Lb2DepthClearHeight(240,1);
  };
  auto stencil_cut=[] {return Lb2StencilClearCutout(0x00305eb6,240,1,1);};
  {auto s=stencil;s.allow_stencil_partial=false;Scope scope(s,true);stencil_vertices();
    Check(!stencil_cut(),"stencil-only extension OFF preserves original imports");}
  for(uint32_t ref:{0u,1u})for(uint32_t color:{0x87000004u,0x87000006u}){
    auto s=stencil;s.stencil_front|=ref;s.color_control=color;Scope scope(s,true);stencil_vertices();
    Check(stencil_cut(),"exact witnessed stencil-only400x240 overwrite admitted");
    Check(!Cutout(),"stencil proof cannot bypass the full-depth proof");
    Check(!Lb2StencilClearCutout(0x00205eb6,240,1,1),"1x stencil target rejected");
    Check(!Lb2StencilClearCutout(0x00305eb6,239,1,1),"short stencil extent rejected");
    Check(!Lb2StencilClearCutout(0x00305eb6,240,2,1),"scaled stencil target rejected");
  }
  auto reject_stencil=[&](State s){Scope scope(s,true);stencil_vertices();Check(!stencil_cut(),"unknown stencil overwrite state rejected");};
  for(auto field:fields)for(unsigned bit=0;bit<32;++bit){auto s=stencil;s.*field^=1u<<bit;
    if(field==&State::stencil_front && bit==0)continue;
    reject_stencil(s);
  }
  for(uint32_t mode:{0u,4u,6u}){auto s=stencil;s.edram_mode=mode;reject_stencil(s);}
  {auto s=stencil;s.has_ps=true;reject_stencil(s);}
  {auto s=stencil;s.memexport=true;reject_stencil(s);}
  {auto s=stencil;s.query=true;reject_stencil(s);}
  {auto s=stencil;s.color_control=0x8700000c;reject_stencil(s);}
  {auto s=stencil;s.program_matches=false;reject_stencil(s);}
  {auto s=stencil;s.rasterized=false;reject_stencil(s);}
  {auto s=stencil;s.half_pixel_offset=false;reject_stencil(s);}
  {auto s=stencil;s.rectangle_draw=false;reject_stencil(s);}
  {auto s=stencil;s.color_mask=1;reject_stencil(s);}
  for(float width:{399.f,401.f,std::numeric_limits<float>::quiet_NaN()}){
    Scope scope(stencil,true);stencil_vertices(width);Check(!stencil_cut(),"inexact stencil width rejected");}
  {Scope scope(stencil,true);stencil_vertices(400,239);Check(!stencil_cut(),"short stencil overwrite rejected");}
  {Scope scope(stencil,true);stencil_vertices(400,240,1);Check(!stencil_cut(),"unknown stencil vertex depth rejected");}
  {Scope scope(stencil,true);stencil_vertices(400,240,0,2);Check(!stencil_cut(),"nonunit stencil W rejected");}
  {Scope scope(stencil,true);stencil_vertices();Lb2DepthClearHeight(240,3);
    Check(!stencil_cut(),"other accumulated render targets rejected");}
  {Scope scope(stencil,false);stencil_vertices();Check(stencil_cut(),"separate stencil opt-in works with full-depth trial OFF");}
  {Scope scope(state,true);Vertices();Check(!stencil_cut(),"full-depth proof cannot crop stencil-only transfer");}
  unsigned stencil_replays=0;
  auto stencil_restore=[](void* p){
    Check(Lb2StencilClearTransferMode(0x00305eb6)==2,"fallback explicitly requests full stencil-only replay");
    ++*static_cast<unsigned*>(p);
  };
  {Scope scope(stencil,true);stencil_vertices();
    Check(Lb2StencilClearTransferMode(0x00305eb6)==0,"proof alone cannot crop transfers");
    Check(Lb2StencilClearDefer(stencil_restore,&stencil_replays),"stencil cutout arms scoped fallback");
    Check(Lb2StencilClearTransferMode(0x00305eb6)==1,"armed normal transfer applies only stencil cutout");
    Check(Lb2StencilClearTransferMode(0x003062d0)==0,"other targets retain full original transfers");
    Lb2DepthClearDrawEmitted(false);
  }
  Check(stencil_replays==1,"placeholder replays full original stencil");
  {Scope scope(stencil,true);stencil_vertices();Lb2StencilClearDefer(stencil_restore,&stencil_replays);
    Lb2DepthClearDrawEmitted(true);}
  Check(stencil_replays==1,"real stencil overwrite commits the cropped import");
  {Scope scope(stencil,true,[](void*){return false;});stencil_vertices();
    Lb2StencilClearDefer(stencil_restore,&stencil_replays);}
  Check(stencil_replays==1,"failed replay submission does not pretend stencil was restored");
  Check(Lb2StencilClearTransferMode(0x00305eb6)==0,"stencil transfer mode cannot leak beyond its draw");
  std::puts("PASS: exact program/state, both pixel centers, scope lifetime and conservative fallback guards");
}
