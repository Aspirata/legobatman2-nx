#pragma once
#include <cstdint>
namespace lb2::stencil_ones {
enum class ShaderMode : uint32_t { kOriginal,kBulk,kResidual };
constexpr uint32_t kModeMask=0xc0000000, kBulkBit=0x80000000, kResidualBit=0x40000000;
constexpr uint32_t Variant(uint32_t key,ShaderMode mode) {
  return (key&~kModeMask) | (mode==ShaderMode::kBulk ? kBulkBit :
                            mode==ShaderMode::kResidual ? kResidualBit : 0);
}
constexpr ShaderMode Decode(uint32_t key) {
  return (key&kModeMask)==kBulkBit ? ShaderMode::kBulk :
         (key&kModeMask)==kResidualBit ? ShaderMode::kResidual : ShaderMode::kOriginal;
}
constexpr bool CanSplit(uint32_t key,bool enabled,bool scoped) {
  return enabled && scoped && !(key&kModeMask) && (key&0xfffff)==0x40102;
}
inline thread_local bool scope_allowed=false;
inline bool ScopeAllowed() { return scope_allowed; }
class Scope {
 public:
  explicit Scope(bool allowed):previous_(scope_allowed) { scope_allowed=allowed; }
  ~Scope() { scope_allowed=previous_; }
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
 private:
  bool previous_;
};
}
