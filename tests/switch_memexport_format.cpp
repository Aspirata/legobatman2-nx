#include "lb2_memexport_format.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>

constexpr uint64_t base = 0x0000000500123456ull;
constexpr uint32_t format = 0x4b026400;

int main() {
  using namespace lb2::memexport;
  unsigned checks = 0;
  auto check = [&](bool ok, const char* text) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
  };
  auto bits = [](float f) { return std::bit_cast<uint32_t>(f); };
  auto make = [&](uint64_t hash = kCopyShaderHash, uint32_t words = 84,
                  uint32_t fmt = format, uint32_t c1x = 0,
                  uint32_t c255x = 0, uint32_t c255z = 0x40800000,
                  uint32_t c255w = 0x41000000, bool vertex = true,
                  uint64_t original = base) {
    return Specialize(original, hash, words, vertex, fmt, c1x, c255x, c255z, c255w);
  };
  uint64_t specialized = make();
  check(HasFormat(specialized), "Copy helper must specialize its invariant export format");
  check(Format(specialized) == format, "All format, number, swizzle and endian bits survive");
  check((specialized & ~kKeyMask) == base, "Existing vertex modification fields survive");
  check(make(kCopyShaderHash ^ 1) == base, "Other shaders use the generic translator");
  check(make(kCopyShaderHash, 83) == base, "Wrong shader length uses the generic translator");
  check(make(kCopyShaderHash, 84, format, 0, 0, 0, 0, false) == base,
        "Expanded and domain vertex inputs use the generic translator");
  check(make(kCopyShaderHash, 84, format ^ 0x100000) == base,
        "Malformed address metadata must not be specialized");
  check(make(kCopyShaderHash, 84, format, 0, bits(1.f)) == base,
        "A variable eA.z must use the generic translator");
  check(make(kCopyShaderHash, 84, format, 0, 0x80000000) == specialized,
        "Negative zero also leaves eA.z invariant");
  check(make(kCopyShaderHash, 84, format, 0, 0, 0, 0, true, base | kKeyMask)
            == (base | kKeyMask), "Occupied key bits must not be overwritten");
  for (uint32_t bad : {0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f7fffffu}) {
    check(make(kCopyShaderHash, 84, format, bad) == base, "Unsafe c1.x falls back");
    check(make(kCopyShaderHash, 84, format, 0, 0, bad) == base, "Unsafe c255.z falls back");
    check(make(kCopyShaderHash, 84, format, 0, 0, 0, bad) == base, "Unsafe c255.w falls back");
  }
  // Every encoded format has a distinct self-contained storage/cache key.
  for (uint32_t f = 0; f < (1u << 20); ++f) {
    uint64_t key = make(kCopyShaderHash, 84, 0x4b000000 | f);
    check(HasFormat(key) && Format(key) == (0x4b000000 | f), "Format key round trip");
    if (f) check(key != make(kCopyShaderHash, 84, 0x4b000000 | (f - 1)), "Formats cannot alias");
  }
  // Compare both fused and unfused MAD against the literal used by the
  // translator, including both exports, negative indices and guard extremes.
  for (float c1 : {-16777216.f, -1.f, 0.f, 1.f, 16777216.f})
    for (float w : {-65536.f, -8.f, -0.f, 0.f, 8.f, 65536.f})
      for (float z : {-16777216.f, -4.f, 0.f, 4.f, 16777216.f})
        for (float index : {-4294967296.f, -1.f, 0.f, 1.f, 16777216.f, 4294967296.f})
          for (uint32_t zero : {0u, 0x80000000u}) {
            auto key = make(kCopyShaderHash, 84, format, bits(c1), zero, bits(z), bits(w));
            check(HasFormat(key), "All bounded coefficients may be specialized");
            float r0 = (index - c1) * w;
            for (unsigned group = 0; group < 2; ++group) {
              float multiplier = std::bit_cast<float>(zero);
              float literal = std::bit_cast<float>(format);
              check(std::isfinite(r0), "Guard keeps both r0 values finite");
              check(bits(std::fma(r0, multiplier, literal)) == Format(key), "Fused eA.z is invariant");
              volatile float product = r0 * multiplier;
              check(bits(product + literal) == Format(key), "Unfused eA.z is invariant");
              r0 += z;
            }
          }
  std::printf("PASS: %u memexport format/cache-key and MAD equivalence checks\n", checks);
}
