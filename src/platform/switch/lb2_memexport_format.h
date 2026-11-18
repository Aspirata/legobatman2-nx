#pragma once

#include <cstdint>

namespace lb2::memexport {

constexpr uint64_t kCopyShaderHash = 0x9e10d1c39a24e18bull;
// Pinned SpirvShaderTranslator::Modification vertex fields occupy bits 0..37.
// Keep this self-contained in the stored translation key (no startup registers).
constexpr uint64_t kFormatFlag = uint64_t{1} << 60;
constexpr uint64_t kKeyMask = kFormatFlag | (uint64_t{0xfffff} << 40);

constexpr bool HasFormat(uint64_t key) { return (key & kFormatFlag) != 0; }
constexpr uint32_t Format(uint64_t key) {
  return 0x4b000000u | uint32_t((key >> 40) & 0xfffff);
}

constexpr uint64_t Specialize(uint64_t original, uint64_t shader_hash,
                              uint32_t dword_count, bool ordinary_vertex,
                              uint32_t c0_z, uint32_t c1_x, uint32_t c255_x,
                              uint32_t c255_z, uint32_t c255_w) {
  if (shader_hash != kCopyShaderHash || dword_count != 84 || !ordinary_vertex ||
      (original & kKeyMask) || (c0_z >> 20) != 0x4b0 ||
      (c255_x & 0x7fffffff) != 0 ||
      (c1_x & 0x7fffffff) > 0x4b800000 ||  // abs(c1.x) <= 2^24
      (c255_z & 0x7fffffff) > 0x4b800000 ||
      (c255_w & 0x7fffffff) > 0x47800000) {  // abs(c255.w) <= 2^16
    return original;
  }
  // This exact shader computes r0=(vertex_index-c1.x)*c255.w, then r0+=c255.z
  // for the second export. Ordinary vertex indices are converted from uint32.
  // The bounds above keep both r0 values finite. Both MADs write
  // eA.z=r0*c255.x+c0.z: adding either signed zero to the positive normal c0.z
  // preserves every bit. Only packing/endian format is specialized; address
  // validation, bounds, memory stores, barriers and readback stay dynamic.
  return original | kFormatFlag | (uint64_t(c0_z & 0xfffff) << 40);
}

}  // namespace lb2::memexport
