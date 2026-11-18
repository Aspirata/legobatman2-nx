#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <tuple>

struct Sample { uint64_t calls; double total, mean, maximum; };
std::map<std::string, Sample> samples;
uint64_t fake_tick = 0;
template <typename... Args> void Capture(const char*, Args... args) {
  if constexpr (sizeof...(Args) == 5) {
    auto values = std::make_tuple(args...);
    samples[std::get<0>(values)] = {std::get<1>(values), std::get<2>(values),
                                   std::get<3>(values), std::get<4>(values)};
  }
}
extern "C" void Lb2GpuProfileTransfersReport() {}
extern "C" void Lb2GpuProfileDrawsReport() {}
extern "C" void Lb2NoEffectPointsReport() {}
extern "C" void Lb2FenceSamplesReport() {}
extern "C" void Lb2DirectColorReport() {}
extern "C" void Lb2DepthClearReport() {}
#include "../src/platform/switch/gpu_profile.cpp"

int main() {
  auto check = [](bool ok, const char* text) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
  };
  Lb2GpuProfileFrame();
  fake_tick = 200;
  Lb2GpuProfileEnd(10, 0);
  fake_tick = 500;
  Lb2GpuProfileEnd(10, 200);
  Lb2GpuProfileEnd(11, 100);
  Lb2GpuProfileEnd(9, 400);
  Lb2GpuProfileEnd(999, 0);
  const bool failed=Lb2GpuProfileCall(12,[&]{fake_tick+=7;return false;});
  int original=42;
  int& reference=Lb2GpuProfileCall(15,[&]() -> int& {fake_tick+=3;return original;});
  check(!failed && &reference==&original,"Profiling calls preserve false results and reference identity");
  fake_tick = 10000;
  Lb2GpuProfileFrame();
  check(samples.size() == 19, "Original, pipeline and memexport timing categories must be reported");
  check(samples["memexport_allocate"].calls==1 && samples["memexport_allocate"].total==7,
        "Failed allocation calls retain exact local CPU duration");
  check(samples["memexport_map"].calls==1 && samples["memexport_map"].total==3,
        "Mapping duration remains separate from the GPU wait");
  check(samples["pipeline_prepare"].calls == 2, "Pipeline preparation calls cannot be discarded");
  check(samples["pipeline_prepare"].total == 500, "Preparation wall times must accumulate");
  check(samples["pipeline_prepare"].mean == 250 && samples["pipeline_prepare"].maximum == 300,
        "Preparation mean and maximum must remain distinct");
  check(samples["pipeline_create"].calls == 1 && samples["pipeline_create"].total == 400,
        "Native pipeline compilation must be recorded separately");
  check(samples["queue_submit"].calls == 1 && samples["queue_submit"].total == 100,
        "Existing queue timings must survive the extension");
  fake_tick = 20000;
  Lb2GpuProfileFrame();
  check(samples["pipeline_prepare"].calls == 0 && samples["pipeline_create"].maximum == 0,
        "Reports must reset all new category statistics");
  std::puts("PASS: production profile categories, totals, maximum, mean, bounds and interval reset");
}
