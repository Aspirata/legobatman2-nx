#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>
#include <nouveau/horizon/nouveau_horizon.h>

uint64_t fake_tick = 0;
unsigned real_calls = 0, stats_calls = 0, reports = 0;
nouveau_horizon_channel* seen_channel;
const nouveau_horizon_exec* seen_execs;
uint32_t seen_count;
nouveau_horizon_channel_stats next_stats{};
std::vector<std::string> last_report;
template<typename T> std::string AsText(T v) {std::ostringstream s;s<<v;return s.str();}
template<typename... Args> void NativeLog(const char*, Args... args) {
  ++reports; last_report={AsText(args)...};
}
extern "C" nouveau_horizon_status __real_nouveau_horizon_channel_exec(
    nouveau_horizon_channel* channel, uint32_t count, const nouveau_horizon_exec* execs) {
  ++real_calls; seen_channel=channel;seen_count=count;seen_execs=execs;
  return NOUVEAU_HORIZON_ERROR_BUSY;
}
extern "C" nouveau_horizon_status __real_nouveau_horizon_channel_submit(
    nouveau_horizon_channel* channel, nouveau_horizon_completion_mode, nouveau_horizon_fence*) {
  ++real_calls;seen_channel=channel;return NOUVEAU_HORIZON_SUCCESS;
}
extern "C" void nouveau_horizon_channel_get_stats(
    nouveau_horizon_channel*, nouveau_horizon_channel_stats* stats) {
  ++stats_calls;*stats=next_stats;
}
#include "../src/platform/switch/native_queue_profile.cpp"
int main() {
  auto check=[](bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}};
  auto* channel=reinterpret_cast<nouveau_horizon_channel*>(uintptr_t(0x1234));
  nouveau_horizon_exec commands[3]{};
  check(__wrap_nouveau_horizon_channel_exec(channel,3,commands)==NOUVEAU_HORIZON_ERROR_BUSY,
        "Exec result must pass through unchanged");
  check(real_calls==1&&seen_channel==channel&&seen_count==3&&seen_execs==commands,
        "Original command pointer/count must be forwarded exactly once");
  check(stats_calls==0&&reports==0,"Fast path must not query stats on every call");
  next_stats.submissions=20;next_stats.submitted_entries=2000;
  next_stats.resource_retries=7;next_stats.resource_recovery_wait_ns=9000000000ull;
  fake_tick=10000;
  __wrap_nouveau_horizon_channel_submit(channel,NOUVEAU_HORIZON_COMPLETION_CPU,nullptr);
  check(real_calls==2&&stats_calls==1&&reports==1,"Submit must report the same channel after ten seconds");
  check(last_report[2]=="20"&&last_report[3]=="2000"&&last_report[4]=="7"&&last_report[6]=="9000",
        "Native pressure counts and milliseconds must be observable");
  check(last_report.back()=="1","First native sample must identify startup totals");
  next_stats.submissions=25;next_stats.submitted_entries=2500;
  next_stats.resource_retries=9;next_stats.resource_recovery_wait_ns=9500000000ull;
  fake_tick=20000;__wrap_nouveau_horizon_channel_exec(channel,3,commands);
  check(last_report[2]=="5"&&last_report[3]=="500"&&last_report[4]=="2"&&last_report[6]=="500",
        "Repeated reports must use interval deltas, not lifetime totals");
  check(last_report.back()=="0","Steady sample must identify interval deltas");
  next_stats={};next_stats.submissions=1;fake_tick=30000;
  __wrap_nouveau_horizon_channel_exec(channel,3,commands);
  check(last_report[2]=="1"&&last_report[6]=="0","Reused channel addresses cannot underflow counters");
  std::puts("PASS: native wrappers preserve commands/results, bounded reporting, interval deltas and counter reset");
}
