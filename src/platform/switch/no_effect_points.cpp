#include "lb2_no_effect_points.h"
#include <rex/logging.h>
#include <atomic>
namespace {
std::atomic<uint64_t> candidates{0},eligible{0},elided{0},query_blocked{0},other_blocked{0};
}
extern "C" bool Lb2TryNoEffectPoint(const lb2::points::State* state,bool enabled) {
  if(!state || state->vs!=lb2::points::kVertexShader)return false;
  candidates.fetch_add(1,std::memory_order_relaxed);
  if(!lb2::points::CanElide(*state)){
    (state->query_active?query_blocked:other_blocked).fetch_add(1,std::memory_order_relaxed);
    return false;
  }
  eligible.fetch_add(1,std::memory_order_relaxed);
  if(!enabled)return false;
  elided.fetch_add(1,std::memory_order_relaxed);
  return true;
}
extern "C" void Lb2NoEffectPointsReport() {
  REXLOG_INFO("LB2 no-effect points: candidate_calls={} eligible_calls={} elided_calls={} "
              "query_blocked={} other_blocked={}; queries, writes and unknown programs retain original path",
              candidates.exchange(0,std::memory_order_relaxed),eligible.exchange(0,std::memory_order_relaxed),
              elided.exchange(0,std::memory_order_relaxed),query_blocked.exchange(0,std::memory_order_relaxed),
              other_blocked.exchange(0,std::memory_order_relaxed));
}
