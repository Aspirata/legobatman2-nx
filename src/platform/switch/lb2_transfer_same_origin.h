#pragma once
#include <cstdint>
namespace lb2::transfer_same_origin {
constexpr uint32_t kModeBit=1u<<30;
constexpr uint32_t MakeKey(uint32_t key,bool eligible) {
  return eligible ? (key & 0xfffff)|kModeBit : key;
}
constexpr bool Eligible(bool enabled,uint32_t key,uint32_t source_base,uint32_t dest_base,
                        uint32_t source_pitch,uint32_t dest_pitch,uint32_t scale_x,uint32_t scale_y) {
  return enabled && (key & 0xfffff)==0x40102 && !(key & 0xc0000000) &&
         source_base==dest_base && source_base<2048 && source_pitch &&
         source_pitch<=1023 && source_pitch==dest_pitch && scale_x==1 && scale_y==1;
}
constexpr bool Rectangle(uint32_t x,uint32_t y,uint32_t width,uint32_t height,uint32_t pitch,
                         uint32_t dest_width,uint32_t dest_height,
                         uint32_t source_width,uint32_t source_height) {
  if(!pitch || pitch>1023 || !width || !height || width>dest_width || height>dest_height ||
     x>dest_width-width || y>dest_height-height ||
     uint64_t(dest_width)!=uint64_t(pitch)*40 || uint64_t(source_width)<uint64_t(dest_width)*2 ||
     height>source_height || y>source_height-height) return false;
  // The original shader wraps the linear tile index at2048. This variant
  // applies only where its quotient/remainder reconstruct the same tile.
  return ((uint64_t(y)+height-1)/8)*pitch+(uint64_t(x)+width-1)/40<2048;
}
}
