#pragma once
#include <cstdint>

extern "C" uint64_t Lb2GpuProfileNow();
extern "C" void Lb2GpuProfileEnd(unsigned category, uint64_t start);
extern "C" void Lb2GpuProfileFrame();
extern "C" void Lb2GpuProfileTransfer(uint32_t shader, uint32_t source, uint32_t dest,
                                     unsigned kind, uint32_t rectangles, uint64_t pixels);
extern "C" void Lb2GpuProfileTransfersReport();
extern "C" void Lb2GpuProfileFastStencilCopy(uint32_t rectangles,uint64_t bytes);
extern "C" void Lb2DirectColorReport();

// Wall time on the calling thread. Nested categories overlap; these are not
// GPU timestamp queries and their times must not be summed.
class Lb2GpuProfileScope {
 public:
  explicit Lb2GpuProfileScope(unsigned category)
      : category_(category), start_(Lb2GpuProfileNow()) {}
  ~Lb2GpuProfileScope() { Lb2GpuProfileEnd(category_, start_); }
  Lb2GpuProfileScope(const Lb2GpuProfileScope&) = delete;
  Lb2GpuProfileScope& operator=(const Lb2GpuProfileScope&) = delete;
 private:
  unsigned category_;
  uint64_t start_;
};
template<class Function>
decltype(auto) Lb2GpuProfileCall(unsigned category,Function&& function) {
  Lb2GpuProfileScope scope(category);
  return static_cast<Function&&>(function)();
}
