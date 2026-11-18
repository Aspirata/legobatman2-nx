#include "lb2_fsi_specialization.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string_view>

int main() {
  using namespace lb2::fsi;
  unsigned checks=0;
  auto check=[&](bool ok,const char* text){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}};
  check(std::string_view(PipelineStorageTag(true)) == "fsi-lb2-v1",
        "Specialized FSI startup must not precompile old generic pipelines");
  check(std::string_view(PipelineStorageTag(false)) == "fbo",
        "Ordinary host render targets must retain their existing pipeline cache");
  constexpr uint64_t base=0x123456789abc;
  std::array<uint32_t,4> formats{0,1,6,7},blends{0x10001,0x10001,0x10001,0x10001};
  check(Specialize(base,false,1,formats,blends)==base,"Host targets must retain their original modification");
  check(Specialize(base,true,0,formats,blends)==base,"Depth-only shaders must remain generic");
  check(Specialize(base,true,16,formats,blends)==base,"Invalid target masks must remain generic");
  check(Specialize(base|kPayloadMask,true,1,formats,blends)==(base|kPayloadMask),"Occupied modification bits cannot be overwritten");
  for(unsigned mask=1;mask<16;++mask) {
    unsigned count=TargetCount(mask),radix=12*BlendRadix(count),limit=1;
    for(unsigned i=0;i<count;++i)limit*=radix;
    check(limit<=32767,"Every complete target tuple fits the free15 bits");
    for(unsigned payload=1;payload<=limit;++payload) {
      unsigned code=payload-1;
      std::array<uint32_t,4> f{},b{};
      for(unsigned i=0;i<4;++i) {
        if(!(mask&(1u<<i))){f[i]=99;b[i]=0;continue;}
        unsigned digit=code%radix;code/=radix;
        f[i]=kFormats[digit%12];
        b[i]=digit/12?kBlends[digit/12]:0x1fff1fff;
      }
      auto key=Specialize(base,true,mask,f,b);
      check((key&~kPayloadMask)==base,"Existing pixel modification fields survive");
      check((key>>49)==payload,"Format/blend tuples cannot alias cache keys");
      check(Valid(key,mask),"Encoded tuple must be self-contained and valid");
      for(unsigned i=0;i<4;++i)if(mask&(1u<<i)) {
        check(Format(key,mask,i)==f[i],"Every written format survives storage/reload");
        check(Blend(key,mask,i)==(b[i]==0x1fff1fff?kDynamic:b[i]),"Every baked blend word survives storage/reload");
      }
    }
    for(unsigned payload=limit+1;payload<=32767;++payload)
      check(!Valid(base|(uint64_t(payload)<<49),mask),"Out-of-range stored payloads must use generic behavior");
    auto good=Specialize(base,true,mask,formats,blends);
    check(good!=base,"Supported formats specialize");
    for(unsigned i=0;i<4;++i) {
      auto changed=formats;changed[i]=99;
      if(mask&(1u<<i))check(Specialize(base,true,mask,changed,blends)==base,"Unknown written formats use the original shader");
      else check(Specialize(base,true,mask,changed,blends)==good,"Unwritten target state must not create variants");
    }
  }
  auto first=Specialize(base,true,1,formats,blends);
  auto changed=blends;changed[0]=0x1fff1fff;
  auto dynamic=Specialize(base,true,1,formats,changed);
  check(first!=dynamic&&Blend(dynamic,1,0)==kDynamic,"Unknown blend words preserve the runtime path");
  changed[0]=blends[0]|~0x1fff1fffu;
  check(Specialize(base,true,1,formats,changed)==first,"Ignored blend-register bits must not create variants");
  check(Format(base,1,0)==kDynamic&&Blend(base,1,0)==kDynamic,"Old generic storage remains compatible");
  std::printf("PASS: %u FSI format/blend, fallback and cache-key checks\n",checks);
}
