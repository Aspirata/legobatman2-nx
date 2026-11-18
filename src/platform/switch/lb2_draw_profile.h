#pragma once
#include <cstdint>

struct Lb2GuestDrawKey {
  uint64_t vs=0, ps=0;
  uint32_t surface=0, depth=0, color0=0, depth_control=0, color_mask=0;
  uint32_t flags=0, primitive=0; // flags:1 memexport,2 rasterization,4 indexed.
  int32_t x=0,y=0;
  uint32_t width=0,height=0;
  bool operator==(const Lb2GuestDrawKey&) const = default;
};
extern "C" void Lb2GpuProfileDraw(const Lb2GuestDrawKey* key,uint32_t vertices_or_indices);
extern "C" void Lb2GpuProfileDrawsReport();
