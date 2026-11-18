#include <rex/logging.h>
#include <switch.h>
#include <mutex>
#include <nouveau/horizon/nouveau_horizon.h>
// Mesa's public include chain defines a legacy macro conflicting with fmt.
#ifdef assume
#undef assume
#endif

extern "C" nouveau_horizon_status __real_nouveau_horizon_channel_exec(
    nouveau_horizon_channel*, uint32_t, const nouveau_horizon_exec*);
extern "C" nouveau_horizon_status __real_nouveau_horizon_channel_submit(
    nouveau_horizon_channel*, nouveau_horizon_completion_mode, nouveau_horizon_fence*);

namespace {
struct Slot {
  nouveau_horizon_channel* channel = nullptr;  // Identity only; never retained or dereferenced.
  uint64_t last_tick = 0;
  bool reported = false;
  nouveau_horizon_channel_stats previous{};
};
Slot slots[16];
std::mutex report_mutex;
uint64_t Delta(uint64_t now, uint64_t previous) { return now >= previous ? now-previous : now; }

void Observe(nouveau_horizon_channel* channel) {
  if (!channel) return;
  // Called only after the original driver operation releases its channel lock.
  // Reading host counters adds no GPU query, fence wait or queue submission.
  std::lock_guard<std::mutex> lock(report_mutex);
  const uint64_t now = armGetSystemTick();
  unsigned index = 0;
  for (; index < 16; ++index) {
    if (slots[index].channel == channel) break;
    if (!slots[index].channel) {
      slots[index].channel = channel;
      slots[index].last_tick = now;
      break;
    }
  }
  if (index == 16) return;
  auto& slot = slots[index];
  const double seconds = double(now-slot.last_tick)/armGetSystemTickFreq();
  if (seconds < 10.0) return;
  nouveau_horizon_channel_stats current{};
  nouveau_horizon_channel_get_stats(channel, &current);
  const auto& before = slot.previous;
  const bool startup_totals = !slot.reported || current.submissions < before.submissions;
  REXLOG_INFO("LB2 native queue: channel={} {:.2f}s submissions={} entries={} retries={} failures={} "
              "recovery_ms={:.2f} credit_ms={:.2f} throttle_ms={:.2f} kickoff_ms={:.2f} "
              "lock_ms={:.2f} mapped_poll_ms={:.2f} exec_ms={:.2f} submit_ms={:.2f} "
              "inflight={} entries_current={} peak_entries_lifetime={} full_barriers={} "
              "gpu_waits={} report_lag_ms={:.2f} recovery_calls={} native_fallbacks={} "
              "mapped_hits={} report_lag_events={} recovery_max_ms_lifetime={:.2f} "
              "startup_totals={}; inclusive CPU times overlap",
              index, seconds, Delta(current.submissions,before.submissions),
              Delta(current.submitted_entries,before.submitted_entries),
              Delta(current.resource_retries,before.resource_retries),
              Delta(current.submit_failures,before.submit_failures),
              Delta(current.resource_recovery_wait_ns,before.resource_recovery_wait_ns)/1e6,
              Delta(current.inflight_credit_wait_ns,before.inflight_credit_wait_ns)/1e6,
              Delta(current.inflight_throttle_wait_ns,before.inflight_throttle_wait_ns)/1e6,
              Delta(current.kickoff_cpu_ns,before.kickoff_cpu_ns)/1e6,
              Delta(current.channel_lock_wait_ns,before.channel_lock_wait_ns)/1e6,
              Delta(current.mapped_completion_poll_ns,before.mapped_completion_poll_ns)/1e6,
              Delta(current.exec_cpu_ns,before.exec_cpu_ns)/1e6,
              Delta(current.submit_cpu_ns,before.submit_cpu_ns)/1e6,
              current.current_inflight_submissions, current.current_inflight_entries,
              current.peak_inflight_entries,
              Delta(current.full_barriers_enqueued,before.full_barriers_enqueued),
              Delta(current.waits_enqueued,before.waits_enqueued),
              Delta(current.mapped_completion_report_lag_wait_ns,before.mapped_completion_report_lag_wait_ns)/1e6,
              Delta(current.resource_recovery_waits,before.resource_recovery_waits),
              Delta(current.mapped_completion_native_fallbacks,before.mapped_completion_native_fallbacks),
              Delta(current.mapped_completion_hits,before.mapped_completion_hits),
              Delta(current.mapped_completion_report_lag_events,before.mapped_completion_report_lag_events),
              current.resource_recovery_max_wait_ns/1e6,
              startup_totals);
  slot.previous = current;
  slot.last_tick = now;
  slot.reported = true;
}
}  // namespace

extern "C" nouveau_horizon_status __wrap_nouveau_horizon_channel_exec(
    nouveau_horizon_channel* channel, uint32_t count, const nouveau_horizon_exec* execs) {
  const auto status = __real_nouveau_horizon_channel_exec(channel,count,execs);
  Observe(channel);
  return status;
}
extern "C" nouveau_horizon_status __wrap_nouveau_horizon_channel_submit(
    nouveau_horizon_channel* channel, nouveau_horizon_completion_mode mode,
    nouveau_horizon_fence* fence) {
  const auto status = __real_nouveau_horizon_channel_submit(channel,mode,fence);
  Observe(channel);
  return status;
}
