#pragma once
#include <cstdint>
#include <vector>
struct Lb2DirectColorPush {
  uint32_t src_x,src_y,width,height,dst_x,dst_y,dst_pitch,dst_base_bytes,endian,swap_rb;
};
static_assert(sizeof(Lb2DirectColorPush)==40);
std::vector<uint32_t> Lb2BuildDirectColorResolveShader();
