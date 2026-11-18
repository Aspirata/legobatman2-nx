#include "lb2_fence_samples.h"
#include <rex/logging.h>
#include <switch.h>
#include <array>
#include <algorithm>
#include <mutex>
namespace {
struct Group {
  Lb2FenceKey key{};bool transfers=false;
  uint64_t samples=0,complete=0,prepare=0,prior=0,min=0,max=0;
  Lb2FenceWork work{};
};
struct Snapshot {
  std::array<Group,128> groups{};
  unsigned used=0;
  uint64_t attempts=0,valid=0,discarded=0,ungrouped=0;
};
Snapshot data,report;
std::mutex mutex,report_mutex;
thread_local Lb2FenceWork* active_work=nullptr;
struct Scheduler {
  bool primed=false,busy=false,failed=false;
  uint64_t born=0,last=0;
  unsigned attempts=0;
  uint32_t random=0x9e3779b9;
} scheduler;
bool Reserve(unsigned kind) {
  std::lock_guard<std::mutex> lock(mutex);
  if(scheduler.failed||scheduler.busy||scheduler.attempts>=256)return false;
  const uint64_t now=armGetSystemTick(),freq=armGetSystemTickFreq();
  if(!scheduler.primed){scheduler.primed=true;scheduler.born=now;}
  if(now-scheduler.born<10*freq)return false;
  scheduler.random^=scheduler.random<<13;scheduler.random^=scheduler.random>>17;scheduler.random^=scheduler.random<<5;
  if(scheduler.random&(kind?7:127))return false;
  if(scheduler.attempts&&now-scheduler.last<freq/8)return false;
  scheduler.last=now;++scheduler.attempts;++data.attempts;scheduler.busy=true;
  return true;
}
void Discard(bool failed) {
  std::lock_guard<std::mutex> lock(mutex);
  ++data.discarded;scheduler.busy=false;scheduler.failed|=failed;
}
void Record(const Lb2FenceKey& key,const Lb2FenceWork& work,uint64_t complete,
            uint64_t prepare,uint64_t prior) {
  std::lock_guard<std::mutex> lock(mutex);
  scheduler.busy=false;++data.valid;
  const bool transfers=work.transfer_draws!=0;
  unsigned i=0;
  for(;i<data.used;++i)if(data.groups[i].key==key&&data.groups[i].transfers==transfers)break;
  if(i==data.used){
    if(i==data.groups.size()){++data.ungrouped;return;}
    ++data.used;data.groups[i].key=key;data.groups[i].transfers=transfers;
  }
  auto& g=data.groups[i];
  if(!g.samples)g.min=complete;else g.min=std::min(g.min,complete);
  ++g.samples;g.complete+=complete;g.prepare+=prepare;g.prior+=prior;g.max=std::max(g.max,complete);
  g.work.guest_draws+=work.guest_draws;g.work.vertices+=work.vertices;
  g.work.transfer_draws+=work.transfer_draws;g.work.transfer_pixels+=work.transfer_pixels;
}
}
Lb2FenceSample::Lb2FenceSample(const Lb2FenceKey& key,bool enabled,Drain drain,void* context)
    :key_(key),drain_(drain),context_(context) {
  if(!enabled||!drain||active_work||!Reserve(key.kind))return;
  const auto before=armGetSystemTick();
  const auto status=drain(context);
  prior_=armGetSystemTick()-before;
  if(status!=Lb2FenceDrain::kComplete){Discard(status==Lb2FenceDrain::kFailed);return;}
  start_=armGetSystemTick();active_=true;active_work=&work_;
}
Lb2FenceSample::~Lb2FenceSample() {
  if(!active_)return;
  if(!committed_){active_work=nullptr;Discard(false);return;}
  const auto prepared=armGetSystemTick();
  const auto status=drain_(context_);
  const auto completed=armGetSystemTick();
  active_work=nullptr;
  if(status!=Lb2FenceDrain::kComplete){Discard(status==Lb2FenceDrain::kFailed);return;}
  Record(key_,work_,completed-start_,prepared-start_,prior_);
}
extern "C" void Lb2FenceSampleGuestDraw(uint32_t vertices) {
  if(active_work){++active_work->guest_draws;active_work->vertices+=vertices;}
}
extern "C" void Lb2FenceSampleTransfer(uint64_t pixels) {
  if(active_work){++active_work->transfer_draws;active_work->transfer_pixels+=pixels;}
}
extern "C" void Lb2FenceSamplesReport() {
  std::lock_guard<std::mutex> serial(report_mutex);
  unsigned lifetime;bool failed;
  {
    std::lock_guard<std::mutex> lock(mutex);
    report=data;data={};lifetime=scheduler.attempts;failed=scheduler.failed;
  }
  REXLOG_INFO("LB2 fence samples: attempts={} valid={} discarded={} attempts_lifetime={} "
              "disabled_after_failure={} ungrouped_samples={}; intrusive isolated CPU+driver+GPU "
              "completion latency, not pure GPU time; prior drain excluded, do not sum or extrapolate",
              report.attempts,report.valid,report.discarded,lifetime,failed,report.ungrouped);
  std::sort(report.groups.begin(),report.groups.begin()+report.used,
            [](const Group& a,const Group& b){return a.complete>b.complete;});
  const double ms=1000.0/armGetSystemTickFreq();
  for(unsigned i=0;i<std::min(report.used,16u);++i){
    const auto& g=report.groups[i];const auto& k=g.key;
    REXLOG_INFO("LB2 fence sample group: kind={} vs={:016X} ps={:016X} surface={:08X} "
                "control={:08X} mask={:08X} extra={:08X} has_transfers={} samples={} "
                "mean_complete_ms={:.3f} mean_prepare_ms={:.3f} mean_prior_ms={:.3f} "
                "min_ms={:.3f} max_ms={:.3f} guest_draws={} vertices_or_indices={} "
                "transfer_draws={} transfer_pixels={}",
                k.kind?"copy":"draw",k.vs,k.ps,k.surface,k.control,k.mask,k.extra,g.transfers,
                g.samples,g.complete*ms/g.samples,g.prepare*ms/g.samples,g.prior*ms/g.samples,
                g.min*ms,g.max*ms,g.work.guest_draws,g.work.vertices,g.work.transfer_draws,g.work.transfer_pixels);
  }
}
