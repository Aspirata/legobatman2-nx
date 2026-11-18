#pragma once
#include <cstdint>
namespace lb2::transfer_pitch {
// Pinned TransferShaderKey uses bits0..19 for mode/formats/sample counts/RT slot.
// Preserve the SDK header and add only a private cache discriminator.
constexpr uint32_t kBaseMask=0xfffff, kPitchShift=20, kMaxPitch=1023;
constexpr uint32_t kSpecializationCacheLimit=128;
constexpr uint32_t BaseKey(uint32_t key) { return key & kBaseMask; }
constexpr uint32_t Pitch(uint32_t key) { return (key >> kPitchShift) & kMaxPitch; }
constexpr uint32_t RemovePitch(uint32_t key) { return key & ~(kMaxPitch << kPitchShift); }
constexpr uint32_t MakeKey(uint32_t base,uint32_t source_pitch,bool enabled) {
  if(!enabled || (base & ~kBaseMask) || !source_pitch || source_pitch>kMaxPitch) return base;
  return base | (source_pitch << kPitchShift);
}
}
