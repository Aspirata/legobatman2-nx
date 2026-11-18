#pragma once
#include <cstdint>
namespace lb2::points {
constexpr uint64_t kVertexShader=0xB6C9863F710683ECull;
constexpr uint64_t kPixelShader=0xA4A965C189287B99ull;
struct State {
  uint64_t vs=0,ps=0;
  uint32_t depth_control=0,color_mask=0;
  bool has_ps=false,vertex_memexport=false,pixel_memexport=false,query_active=false;
  bool point_list=false,auto_index=false,normal_vertex_path=false,normal_export=false;
};
constexpr bool CanElide(const State& s) {
  return s.vs==kVertexShader && (s.has_ps?s.ps==kPixelShader:s.ps==0) &&
         s.depth_control==0 && s.color_mask==0 && !s.vertex_memexport &&
         !s.pixel_memexport && !s.query_active && s.point_list && s.auto_index &&
         s.normal_vertex_path && s.normal_export;
}
}
extern "C" bool Lb2TryNoEffectPoint(const lb2::points::State* state,bool enabled);
extern "C" void Lb2NoEffectPointsReport();
