#pragma once
#include <cstdint>
namespace lb2::dump_pitch {
// Pinned SDK DumpPipelineKey: samples[1:0], format[5:2], depth[6].
// Keep its public header and object-cache ABI unchanged.
constexpr uint32_t kBaseMask=127, kPitchShift=7, kMaxPitch=1023;
constexpr uint32_t kSpecializationCacheLimit=64;
constexpr uint32_t BaseKey(uint32_t key) { return key & kBaseMask; }
constexpr uint32_t Pitch(uint32_t key) { return (key >> kPitchShift) & kMaxPitch; }
constexpr uint32_t MakeKey(uint32_t base,uint32_t source_pitch,bool enabled) {
  if(!enabled || (base & ~kBaseMask) || !source_pitch || source_pitch>kMaxPitch) return base;
  return base | (source_pitch << kPitchShift);
}
}
