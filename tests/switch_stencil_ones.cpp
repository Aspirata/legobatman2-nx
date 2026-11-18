#include "lb2_stencil_ones.h"
#include "lb2_transfer_pitch.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unordered_set>
int main() {
  unsigned checks=0;
  auto check=[&](bool value,const char* message) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
  };
  using namespace lb2::stencil_ones;
  check(!ScopeAllowed(),"Optimization must be forbidden outside a guarded draw");
  {
    Scope draw(true);check(ScopeAllowed(),"Query-free draw may enable splitting");
    {Scope copy(false);check(!ScopeAllowed(),"Copy or active-query nested scope forbids splitting");}
    check(ScopeAllowed(),"Nested scope restores the enclosing draw");
    bool inherited=true;
    std::thread worker([&](){inherited=ScopeAllowed();Scope inner(true);});worker.join();
    check(!inherited&&ScopeAllowed(),"Scope permission cannot leak across threads");
  }
  check(!ScopeAllowed(),"Draw exit resets scope permission");
  auto early=[](){Scope draw(true);return ScopeAllowed();};
  check(early()&&!ScopeAllowed(),"Early return restores the caller scope");
  std::unordered_set<uint32_t> keys;
  for(uint32_t pitch=0;pitch<=1023;++pitch) {
    auto original=lb2::transfer_pitch::MakeKey(0x40102,pitch,true);
    check(CanSplit(original,true,true),"Known color2x-to-stencil4x mode remains eligible with any supported pitch");
    check(!CanSplit(original,false,true)&&!CanSplit(original,true,false),"Flag and query-free scope are both required");
    for(auto mode:{ShaderMode::kOriginal,ShaderMode::kBulk,ShaderMode::kResidual}) {
      auto key=Variant(original,mode);
      check(Decode(key)==mode,"Shader mode must survive cache key encoding");
      check((key&0x3fffffff)==original,"New mode bits cannot alter source pitch or original transfer fields");
      check(keys.insert(key).second,"Original/bulk/residual cache identities must be distinct");
      auto generic=lb2::transfer_pitch::RemovePitch(key);
      check(Decode(generic)==mode&&lb2::transfer_pitch::Pitch(generic)==0,
            "Cache pressure fallback must retain stencil mode while removing only source pitch");
      check(lb2::transfer_pitch::BaseKey(generic)==0x40102,"Fallback preserves original transfer semantics");
    }
    check(!CanSplit(Variant(original,ShaderMode::kBulk),true,true),"Already tagged requests cannot recursively split");
  }
  for(unsigned bit=0;bit<20;++bit)
    check(!CanSplit(0x40102^(1u<<bit),true,true),"Unmeasured modes/formats/sample layouts fail closed");
  check(Decode(0xc0040102)==ShaderMode::kOriginal,"Invalid combined mode fails closed");
  std::printf("PASS: %u stencil split eligibility, cache-mode and query-scope checks\n",checks);
}
