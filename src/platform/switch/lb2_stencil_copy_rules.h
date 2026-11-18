#pragma once
#include <cstdint>

namespace lb2::stencil {
struct Target {uint32_t base,pitch,format,samples;bool depth;};
struct Push {uint32_t x,y,width,height,row_words;};
static_assert(sizeof(Push)==20);
struct Plan {Push push{};uint32_t groups=0;uint64_t bytes=0;};
constexpr bool Eligible(bool enabled,const Target& source,const Target& dest,
                        uint32_t scale_x,uint32_t scale_y,bool standard_samples) {
  return enabled&&standard_samples&&scale_x==1&&scale_y==1&&source.depth&&dest.depth&&
         source.samples==2&&dest.samples==0&&source.pitch&&source.base==dest.base&&
         source.pitch==dest.pitch&&source.format==dest.format;
}
constexpr bool MakePlan(uint32_t x,uint32_t y,uint32_t width,uint32_t height,
                        uint32_t dest_width,uint32_t dest_height,
                        uint32_t source_width,uint32_t source_height,
                        uint64_t capacity,Plan& plan) {
  if(!width||!height||width>UINT32_MAX-3||width>dest_width||height>dest_height||
     x>dest_width-width||y>dest_height-height||
     (uint64_t(x)+width-1)/2>=source_width||
     (uint64_t(y)+height-1)/2>=source_height)return false;
  // The generic SDK wraps local tile indices at2048. Its exact transfer
  // rectangles exclude the padded tail, but keep this guard fail-closed.
  if(!dest_width||dest_width%80||
     ((uint64_t(y)+height-1)/16)*(dest_width/80)+(uint64_t(x)+width-1)/80>=2048)return false;
  uint32_t row_words=(width+3)/4;
  uint64_t words=uint64_t(row_words)*height,groups=(words+63)/64;
  if(words>capacity/4||!groups||groups>65535)return false;
  plan={{x,y,width,height,row_words},uint32_t(groups),words*4};
  return true;
}
}
