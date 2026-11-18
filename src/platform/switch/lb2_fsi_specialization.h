#pragma once
#include <array>
#include <cstdint>

namespace lb2::fsi {
// A separate file prevents eager startup compilation of unused generic FSI
// pipelines. Preserve the baseline cache for rollback; guest/native caches
// remain shared. Change this tag if the persistent payload encoding changes.
constexpr const char* PipelineStorageTag(bool interlock) {
  return interlock ? "fsi-lb2-v1" : "fbo";
}
// The pinned pixel Modification uses bits 0..48. The payload is self-contained
// in persistent shader/pipeline keys; zero retains the original runtime path.
constexpr uint64_t kPayloadMask = uint64_t{0x7fff} << 49;
constexpr uint32_t kDynamic = UINT32_MAX;
// IDs are persistent. Never reorder these tables without invalidating storage.
constexpr std::array<uint32_t,12> kFormats{0,1,2,3,4,5,6,7,10,12,14,15};
constexpr uint32_t BlendWord(unsigned src, unsigned dst, unsigned alpha_src,
                              unsigned alpha_dst, unsigned op=0, unsigned alpha_op=0) {
  return src | (op<<5) | (dst<<8) | (alpha_src<<16) | (alpha_op<<21) | (alpha_dst<<24);
}
constexpr std::array<uint32_t,32> kBlends{
  kDynamic,
  BlendWord(1,0,1,0), BlendWord(6,7,6,7), BlendWord(1,7,1,7), BlendWord(1,1,1,1),
  BlendWord(6,7,1,7), BlendWord(6,1,1,1), BlendWord(8,0,8,0),
  BlendWord(6,7,1,0), BlendWord(6,1,6,1), BlendWord(0,4,0,4), BlendWord(1,0,0,1),
  BlendWord(6,7,0,1), BlendWord(1,7,1,0), BlendWord(1,1,1,0), BlendWord(8,0,1,0),
  BlendWord(1,1,0,1), BlendWord(0,1,0,1), BlendWord(0,0,0,0), BlendWord(10,11,10,11),
  BlendWord(1,9,1,9), BlendWord(12,13,12,13), BlendWord(14,15,14,15), BlendWord(4,5,4,5),
  BlendWord(16,1,16,1), BlendWord(1,1,1,1,1,1), BlendWord(1,1,1,1,4,4),
  BlendWord(1,1,1,1,2,2), BlendWord(1,1,1,1,3,3), BlendWord(1,1,1,0,1,0),
  BlendWord(1,1,1,0,4,0), BlendWord(8,4,8,4)
};
constexpr unsigned TargetCount(unsigned mask) {
  unsigned n=0;for(;mask;mask>>=1)n+=mask&1;return n;
}
constexpr unsigned BlendRadix(unsigned targets) {
  return targets==1?32:targets==2?8:targets==3?2:1;
}
constexpr unsigned PayloadLimit(unsigned mask) {
  if(!mask||(mask&~15u))return 0;
  unsigned n=TargetCount(mask),radix=12*BlendRadix(n),limit=1;
  for(unsigned i=0;i<n;++i)limit*=radix;
  return limit;
}
constexpr bool Valid(uint64_t key,unsigned mask) {
  unsigned payload=unsigned(key>>49);
  return payload&&payload<=PayloadLimit(mask);
}
constexpr unsigned Digit(uint64_t key,unsigned mask,unsigned target) {
  if(target>=4||!(mask&(1u<<target))||!Valid(key,mask))return kDynamic;
  unsigned code=unsigned(key>>49)-1,radix=12*BlendRadix(TargetCount(mask));
  for(unsigned rank=TargetCount(mask&((1u<<target)-1));rank;--rank)code/=radix;
  return code%radix;
}
constexpr uint32_t Format(uint64_t key,unsigned mask,unsigned target) {
  unsigned digit=Digit(key,mask,target);
  return digit==kDynamic?kDynamic:kFormats[digit%12];
}
constexpr uint32_t Blend(uint64_t key,unsigned mask,unsigned target) {
  unsigned digit=Digit(key,mask,target);
  return digit==kDynamic?kDynamic:kBlends[digit/12];
}
constexpr uint64_t Specialize(uint64_t original,bool interlock,unsigned written_targets,
                               const std::array<uint32_t,4>& formats,
                               const std::array<uint32_t,4>& blends) {
  if(!interlock||!PayloadLimit(written_targets)||(original&kPayloadMask))return original;
  unsigned blend_radix=BlendRadix(TargetCount(written_targets)),radix=12*blend_radix;
  unsigned code=0,multiplier=1;
  for(unsigned target=0;target<4;++target) {
    if(!(written_targets&(1u<<target)))continue;
    unsigned format=0;
    while(format<kFormats.size()&&kFormats[format]!=formats[target])++format;
    if(format==kFormats.size())return original;
    unsigned blend=0;
    for(unsigned i=1;i<blend_radix;++i)if(kBlends[i]==(blends[target]&0x1fff1fff)) {blend=i;break;}
    code+=(format+12*blend)*multiplier;multiplier*=radix;
  }
  return original|(uint64_t(code+1)<<49);
}
}  // namespace lb2::fsi
