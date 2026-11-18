#include "lb2_gpu_profile.h"
#include "lb2_draw_profile.h"
#include "lb2_no_effect_points.h"
#include "lb2_fence_samples.h"
#include "lb2_depth_clear.h"
#include <rex/logging.h>
#include <switch.h>
#include <atomic>

namespace {
struct Counter {
  std::atomic<uint64_t> calls{0}, ticks{0}, max_ticks{0};
};
constexpr const char* names[] = {"draw", "memexport", "submit", "fence", "resolve", "textures",
                               "encode", "end_command_buffer", "queue_acquire", "queue_submit",
                               "pipeline_prepare", "pipeline_create", "memexport_allocate",
                               "memexport_record_copy", "memexport_wait", "memexport_map",
                               "memexport_invalidate", "memexport_memcpy", "memexport_release"};
constexpr unsigned category_count = sizeof(names) / sizeof(names[0]);
Counter counters[category_count];
}
extern "C" uint64_t Lb2GpuProfileNow() { return armGetSystemTick(); }
// The inherited skip-work test invalidates memexport/texture contents in LB2.
// Keep diagnostics observational so this shortcut cannot corrupt the live frame.
extern "C" void __wrap_RexSwitchPerfToggleAb() {
  REXLOG_WARN("LB2: legacy GPU skip-work test disabled; using observational GPU timings");
}
extern "C" void Lb2GpuProfileEnd(unsigned category, uint64_t start) {
  if (category >= category_count) return;
  const uint64_t ticks = armGetSystemTick() - start;
  auto& counter = counters[category];
  counter.calls.fetch_add(1, std::memory_order_relaxed);
  counter.ticks.fetch_add(ticks, std::memory_order_relaxed);
  auto previous = counter.max_ticks.load(std::memory_order_relaxed);
  while (previous < ticks && !counter.max_ticks.compare_exchange_weak(
      previous, ticks, std::memory_order_relaxed)) {}
}
extern "C" void Lb2GpuProfileFrame() {
  // VdSwap runs on one guest presentation thread. Counter producers are atomic.
  static uint64_t previous = armGetSystemTick();
  static unsigned frames = 0;
  ++frames;
  const auto now = armGetSystemTick();
  const double seconds = double(now - previous) / armGetSystemTickFreq();
  if (seconds < 10.0) return;
  previous = now;
  REXLOG_INFO("LB2 GPU profile: {:.2f}s, {} guest frames; approximate interval, inclusive calling-thread wall times (overlap)", seconds, frames);
  frames = 0;
  Lb2GpuProfileTransfersReport();
  Lb2GpuProfileDrawsReport();
  Lb2NoEffectPointsReport();
  Lb2FenceSamplesReport();
  Lb2DirectColorReport();
  Lb2DepthClearReport();
  for (unsigned i = 0; i < category_count; ++i) {
    const auto calls = counters[i].calls.exchange(0, std::memory_order_relaxed);
    const auto ticks = counters[i].ticks.exchange(0, std::memory_order_relaxed);
    const auto max_ticks = counters[i].max_ticks.exchange(0, std::memory_order_relaxed);
    const double milliseconds = double(ticks) * 1000.0 / armGetSystemTickFreq();
    REXLOG_INFO("LB2 GPU profile {}: {} calls, {:.2f}ms total, {:.3f}ms mean, {:.2f}ms max",
                names[i], calls, milliseconds, calls ? milliseconds / calls : 0.0,
                double(max_ticks) * 1000.0 / armGetSystemTickFreq());
  }
}
