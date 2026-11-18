#include <rex/ui/vulkan/device.h>
#include "lb2_gpu_timestamps.h"
#include <rex/logging.h>
#include <rex/cvar.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <compare>
#include <limits>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <cctype>

REXCVAR_DEFINE_BOOL(lb2_gpu_timestamps, true, "GPU/Profiling",
                    "Emit sparse GPU timing queries in a diagnostic build")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(lb2_gpu_phase_profile, false, "GPU/Profiling",
                    "Time contiguous GPU workload phases in1/32 command buffers, with delayed reads")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(lb2_gpu_phase_coarse, false, "GPU/Profiling",
                    "Group phase timings by RT960 guest sequences including intervening helpers")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {
using Device = rex::ui::vulkan::VulkanDevice;
constexpr unsigned kSlots = 128, kMaxPages = 64, kNoSample = UINT32_MAX;
struct Key {
  Lb2GpuTimestampKind kind;
  uint64_t vs = 0, ps = 0, internal_pipeline = 0;
  auto operator<=>(const Key&) const = default;
};
struct Stats {
  uint64_t observed=0, samples=0, work=0, operations=0, selected=0, recorded=0;
  double ns=0, max_ns=0;
};
struct Sample { Key key; unsigned first; uint64_t work; uint64_t operations=1; };
enum PhaseRole : unsigned { kGuest960,kGuestOther,kOwnership,kDump,kResolve,kTexture,kHostDepth,kSwap,kCopyClear,kOtherDraw,kOtherCompute,kOtherGpu,kRt960Sequence };
constexpr const char* phase_names[]={"guest_rt960","guest_other","ownership","edram_dump","resolve","texture_load","host_depth","swap","copy_clear","other_draw","other_compute","other_gpu","rt960_sequence"};
struct Page {
  VkQueryPool pool = VK_NULL_HANDLE;
  uint64_t submission = 0;
  bool pending = false;
  unsigned used = 0;
  std::vector<Sample> samples;
};
struct State {
  const Device* device = nullptr;
  bool enabled = false;
  bool phase_mode=false;
  bool phase_coarse=false;
  uint64_t phase_buffers=0,phase_selected=0,phase_truncated=0,last_phase_poll=UINT64_MAX;
  uint64_t phase_first_submission=0,phase_last_submission=0;
  uint32_t phase_random=0x7f4a7c15;
  PFN_vkCmdWriteTimestamp write_timestamp = nullptr;
  PFN_vkGetQueryPoolResults get_results = nullptr;
  unsigned valid_bits = 0;
  double period = 0;
  uint32_t random = 0x9e3779b9;
  uint64_t skipped = 0;
  unsigned pipeline_name_reports = 0;
  std::vector<std::unique_ptr<Page>> pages;
  std::map<VkPipeline, std::pair<uint64_t, uint64_t>> labels;
  std::map<VkPipeline,unsigned> roles;
  std::map<Key, Stats> stats;
  std::chrono::steady_clock::time_point report_time = std::chrono::steady_clock::now();
} state;

void Initialize(const Device* device) {
  if (state.device) return;
  state.device = device;
  state.phase_mode=REXCVAR_GET(lb2_gpu_phase_profile);
  state.phase_coarse=state.phase_mode && REXCVAR_GET(lb2_gpu_phase_coarse);
  if (!REXCVAR_GET(lb2_gpu_timestamps) && !state.phase_mode) {
    REXLOG_INFO("LB2 GPU timestamps: disabled by startup setting; no query commands");
    return;
  }
  const auto& dfn = device->functions();
  if (!dfn.vkCreateQueryPool || !dfn.vkDestroyQueryPool || !dfn.vkCmdResetQueryPool) return;
  const auto& ifn = device->vulkan_instance()->functions();
  if (!ifn.vkGetDeviceProcAddr) return;
  state.write_timestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(
      ifn.vkGetDeviceProcAddr(device->device(), "vkCmdWriteTimestamp"));
  state.get_results = reinterpret_cast<PFN_vkGetQueryPoolResults>(
      ifn.vkGetDeviceProcAddr(device->device(), "vkGetQueryPoolResults"));
  if (!state.write_timestamp || !state.get_results) return;
  VkPhysicalDeviceProperties props{};
  ifn.vkGetPhysicalDeviceProperties(device->physical_device(), &props);
  uint32_t count = 0;
  ifn.vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device(), &count, nullptr);
  if (!count || device->queue_family_graphics_compute() >= count) return;
  std::vector<VkQueueFamilyProperties> queues(count);
  ifn.vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device(), &count, queues.data());
  state.valid_bits = queues[device->queue_family_graphics_compute()].timestampValidBits;
  state.period = props.limits.timestampPeriod;
  state.enabled = state.valid_bits > 0 && state.valid_bits <= 64 &&
                  std::isfinite(state.period) && state.period > 0;
  REXLOG_INFO("LB2 GPU timestamps: enabled={} valid_bits={} period_ns={:.3f}; "
              "sparse intrusive samples, no extra CPU waits", state.enabled, state.valid_bits,
              state.period);
}

void Add(const Key& key, uint64_t start, uint64_t end, uint64_t work,uint64_t operations=1) {
  uint64_t delta = end - start;
  if (state.valid_bits < 64) delta &= (uint64_t{1} << state.valid_bits) - 1;
  auto& stats = state.stats[key];
  ++stats.samples; stats.work += work;
  stats.operations+=operations;
  double ns = double(delta) * state.period;
  stats.ns += ns; stats.max_ns = std::max(stats.max_ns, ns);
}

Key MakeKey(Lb2GpuTimestampKind kind, VkPipeline pipeline) {
  Key key{kind};
  if (kind == Lb2GpuTimestampKind::kDraw || kind == Lb2GpuTimestampKind::kIndexed) {
    auto label = state.labels.find(pipeline);
    if (label != state.labels.end()) {
      key.vs = label->second.first; key.ps = label->second.second; return key;
    }
  }
  key.internal_pipeline = uint64_t(pipeline);
  return key;
}
}  // namespace

void Lb2GpuTimestampsRole(VkPipeline pipeline,const char* role) {
  if(pipeline==VK_NULL_HANDLE || !role)return;
  if (REXCVAR_GET(lb2_gpu_phase_profile)) {
    std::string name(role);
    for(char& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
    unsigned kind=kNoSample;
    if(name.find("ownership")!=std::string::npos)kind=kOwnership;
    else if(name.find("dump")!=std::string::npos)kind=kDump;
    else if(name.find("resolve")!=std::string::npos || name=="direct_color_rgba8")kind=kResolve;
    else if(name.find("texture")!=std::string::npos)kind=kTexture;
    else if(name.find("host_depth")!=std::string::npos)kind=kHostDepth;
    else if(name.find("swap")!=std::string::npos)kind=kSwap;
    if(kind==kNoSample)state.roles.erase(pipeline);
    else if(state.roles.size()<8192 || state.roles.contains(pipeline))state.roles[pipeline]=kind;
    state.labels.erase(pipeline);
  }
}
extern "C" void Lb2GpuTimestampsPipelineName(VkPipeline pipeline, const char* role,
                                             uint64_t key, uint64_t variant) {
  Lb2GpuTimestampsRole(pipeline,role);
  if (pipeline == VK_NULL_HANDLE || !role || state.pipeline_name_reports >= 2048) return;
  ++state.pipeline_name_reports;
  REXLOG_INFO("LB2 GPU pipeline name: internal={:016X} role={} key={:016X} variant={:016X}",
              uint64_t(pipeline), role, key, variant);
}

Lb2GpuTimestampBatch::Lb2GpuTimestampBatch(const Device* device, VkCommandBuffer command,
                                           uint64_t submission) : command_(command) {
  Initialize(device);
  if (state.device != device || !state.enabled) return;
  phase_enabled_=state.phase_mode;
  if(phase_enabled_){
    ++state.phase_buffers;++state.stats[{Lb2GpuTimestampKind::kCommandBuffer}].observed;
    state.phase_random^=state.phase_random<<13;state.phase_random^=state.phase_random>>17;state.phase_random^=state.phase_random<<5;
    if((state.phase_buffers!=1 && (state.phase_random & 31)) || state.phase_selected>=64)return;
  }
  Page* page = nullptr;
  for (auto& candidate : state.pages) {
    if (!candidate->pending) { page = candidate.get(); break; }
  }
  if (!page) {
    if (state.pages.size() == (state.phase_mode ? 8u : kMaxPages)) { ++state.skipped; return; }
    auto candidate = std::make_unique<Page>();
    VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    info.queryType = VK_QUERY_TYPE_TIMESTAMP; info.queryCount = kSlots;
    if (device->functions().vkCreateQueryPool(device->device(), &info, nullptr,
                                              &candidate->pool) != VK_SUCCESS) {
      state.enabled = false;
      REXLOG_WARN("LB2 GPU timestamps: query allocation failed, sampling disabled"); return;
    }
    candidate->samples.reserve((kSlots - 2) / 2);
    page = candidate.get(); state.pages.push_back(std::move(candidate));
  }
  page_ = page; page->submission = submission; page->pending = true;
  if(phase_enabled_){
    if(!state.phase_selected)state.phase_first_submission=submission;
    state.phase_last_submission=submission;
    ++state.phase_selected;
  }
  page->used = 1; page->samples.clear();
  const auto& dfn = device->functions();
  // Execute starts outside any render pass; no page is allocated or reset in
  // Begin(), which can run inside a render pass.
  dfn.vkCmdResetQueryPool(command_, page->pool, 0, kSlots);
  state.write_timestamp(command_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, page->pool, 0);
  if(!phase_enabled_)++state.stats[{Lb2GpuTimestampKind::kCommandBuffer}].observed;
}

Lb2GpuTimestampBatch::~Lb2GpuTimestampBatch() {
  auto* page = static_cast<Page*>(page_);
  if (!page) return;
  EndPhase();
  state.write_timestamp(command_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                page->pool, page->used++);
}

void Lb2GpuTimestampBatch::Bind(VkPipelineBindPoint point, VkPipeline pipeline) {
  if (point == VK_PIPELINE_BIND_POINT_GRAPHICS) graphics_ = pipeline;
  else if (point == VK_PIPELINE_BIND_POINT_COMPUTE) compute_ = pipeline;
}

unsigned Lb2GpuTimestampBatch::Begin(Lb2GpuTimestampKind kind, uint64_t work) {
  if(phase_enabled_){Phase(kind,work);return kNoSample;}
  auto* page = static_cast<Page*>(page_);
  if (!page || kind == Lb2GpuTimestampKind::kCommandBuffer) return kNoSample;
  VkPipeline pipeline = kind == Lb2GpuTimestampKind::kDispatch ? compute_ : graphics_;
  if (kind == Lb2GpuTimestampKind::kCopy) pipeline = VK_NULL_HANDLE;
  Key key = MakeKey(kind, pipeline);
  if (state.stats.size() >= 2048 && !state.stats.contains(key)) return kNoSample;
  auto& stats = state.stats[key];
  ++stats.observed;
  // Rotating pseudorandom sampling avoids phase-locking to a fixed draw order.
  state.random ^= state.random << 13; state.random ^= state.random >> 17;
  state.random ^= state.random << 5;
  unsigned mask = kind == Lb2GpuTimestampKind::kDispatch ? 15 :
                  kind == Lb2GpuTimestampKind::kCopy ? 31 : 127;
  if ((stats.observed != 1 && (state.random & mask)) || page->used + 3 > kSlots) return kNoSample;
  unsigned token = unsigned(page->samples.size());
  page->samples.push_back({key, page->used, work}); page->used += 2;
  state.write_timestamp(command_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                page->pool, page->samples[token].first);
  return token;
}

void Lb2GpuTimestampBatch::EndPhase(){
  if(phase_token_!=kNoSample){End(phase_token_);phase_token_=kNoSample;}
}
void Lb2GpuTimestampBatch::Phase(Lb2GpuTimestampKind kind,uint64_t work){
  if(!state.enabled)return;
  VkPipeline pipeline=kind==Lb2GpuTimestampKind::kDispatch?compute_:graphics_;
  unsigned role=kind==Lb2GpuTimestampKind::kDispatch?kOtherCompute:kOtherDraw;
  if(kind==Lb2GpuTimestampKind::kCopy)role=kCopyClear;
  else if(auto named=state.roles.find(pipeline);named!=state.roles.end())role=named->second;
  else if((kind==Lb2GpuTimestampKind::kDraw || kind==Lb2GpuTimestampKind::kIndexed) && state.labels.contains(pipeline))
    role=render_width_==960?kGuest960:kGuestOther;
  const Key census_key{Lb2GpuTimestampKind::kPhase,role};
  auto& census=state.stats[census_key];
  ++census.observed;
  if(state.phase_coarse){
    // Keep all helper work inside a guest sequence. Texture dispatches and
    // ownership draws must not fragment the late960 group into query-sized
    // prefixes. This interval includes helpers; it is not pure guest draw time.
    if(role==kGuest960 || role==kGuestOther)in_rt960_sequence_=role==kGuest960;
    role=in_rt960_sequence_?kRt960Sequence:kOtherGpu;
  }
  const Key key{Lb2GpuTimestampKind::kPhase,role};
  auto& stats=state.stats[key];
  if(key!=census_key)++stats.observed;
  auto* page=static_cast<Page*>(page_);
  if(!page)return;
  ++stats.selected;
  if(key!=census_key)++census.selected;
  if(role!=phase_role_){
    EndPhase();phase_role_=role;
    if(page->used+3<=kSlots){
      phase_token_=unsigned(page->samples.size());
      page->samples.push_back({key,page->used,0,0});page->used+=2;
      state.write_timestamp(command_,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,page->pool,page->samples[phase_token_].first);
    }else ++state.phase_truncated;
  }
  if(phase_token_!=kNoSample){
    auto& sample=page->samples[phase_token_];sample.work+=work;++sample.operations;
    ++stats.recorded;
    if(key!=census_key)++census.recorded;
  }
}

void Lb2GpuTimestampBatch::End(unsigned token) {
  auto* page = static_cast<Page*>(page_);
  if (!page || token == kNoSample || token >= page->samples.size()) return;
  state.write_timestamp(command_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                page->pool, page->samples[token].first + 1);
}

void Lb2GpuTimestampsLabel(VkPipeline pipeline, uint64_t vs, uint64_t ps) {
  if (!REXCVAR_GET(lb2_gpu_timestamps) && !REXCVAR_GET(lb2_gpu_phase_profile)) return;
  if (state.labels.size() < 8192 || state.labels.contains(pipeline)) state.labels[pipeline] = {vs, ps};
  if(REXCVAR_GET(lb2_gpu_phase_profile))state.roles.erase(pipeline);
}
void Lb2GpuTimestampsForget(VkPipeline pipeline) { state.labels.erase(pipeline);state.roles.erase(pipeline); }
void Lb2GpuTimestampsAbandon() {
  state.enabled = false;
  state.stats.clear();
  REXLOG_WARN("LB2 GPU timestamps: discarded measurements after a command/query failure");
}

void Lb2GpuTimestampsCompleted(const Device* device, uint64_t completed) {
  if (state.device != device || !state.enabled) return;
  if(state.phase_mode){if(state.last_phase_poll==completed)return;state.last_phase_poll=completed;}
  std::array<std::array<uint64_t, 2>, kSlots> values;
  for (auto& page : state.pages) {
    if (!page->pending || page->submission > completed) continue;
    if(state.phase_mode && completed-page->submission<4)continue;
    VkResult result = state.get_results(
        device->device(), page->pool, 0, page->used, sizeof(values), values.data(), sizeof(values[0]),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if (result == VK_NOT_READY) continue;
    if (result != VK_SUCCESS) {
      Lb2GpuTimestampsAbandon();
      REXLOG_WARN("LB2 GPU timestamps: result={} sampling disabled", int(result)); return;
    }
    bool available = true;
    for (unsigned i = 0; i < page->used; ++i) available &= values[i][1] != 0;
    if (!available) continue;
    Add({Lb2GpuTimestampKind::kCommandBuffer}, values[0][0], values[page->used - 1][0], 0);
    for (const auto& sample : page->samples) {
      Add(sample.key, values[sample.first][0], values[sample.first + 1][0], sample.work,sample.operations);
    }
    page->pending = false;
  }
  if (std::chrono::steady_clock::now() - state.report_time >= std::chrono::seconds(10)) {
    Lb2GpuTimestampsReport();
  }
}

void Lb2GpuTimestampsReport() {
  constexpr const char* names[]{"draw", "indexed", "dispatch", "copy", "command_buffer","phase"};
  std::vector<std::pair<Key, Stats>> samples;
  for (const auto& [key, stats] : state.stats)
    if (stats.samples || (key.kind==Lb2GpuTimestampKind::kPhase && stats.observed))
      samples.emplace_back(key, stats);
  std::sort(samples.begin(), samples.end(), [](const auto& a, const auto& b) {
    return a.second.ns > b.second.ns;
  });
  if(state.phase_mode) REXLOG_INFO("LB2 GPU phase coverage: buffers_seen={} selected={} truncated_segments={} groups={} first_selected_submission={} last_selected_submission={} buffers_read={} coarse_rt960={};1/32 buffers,max64,read_lag4,cumulative GPU segments,not frame percentages",
    state.phase_buffers,state.phase_selected,state.phase_truncated,samples.size(),
    state.phase_first_submission,state.phase_last_submission,
    state.stats[{Lb2GpuTimestampKind::kCommandBuffer}].samples,state.phase_coarse);
  REXLOG_INFO("LB2 GPU timestamps: {} groups, {} skipped buffers; "
              "TOP->BOTTOM buffer spans, sparse BOTTOM->BOTTOM operation samples; "
              "intrusive/overlapping, do not sum or extrapolate", samples.size(), state.skipped);
  unsigned printed = 0;
  for (const auto& [key, stats] : samples) {
    if (printed++ == 16) break;
    if(key.kind==Lb2GpuTimestampKind::kPhase){
      REXLOG_INFO("LB2 GPU phase role={} observed_operations={} samples={} sampled_operations={} selected_operations={} recorded_operations={} sampled_ms={:.3f} mean_us={:.3f} max_us={:.3f}",
        phase_names[key.vs],stats.observed,stats.samples,stats.operations,stats.selected,stats.recorded,stats.ns/1e6,
        stats.samples?stats.ns/stats.samples/1e3:0.0,stats.max_ns/1e3);
      continue;
    }
    REXLOG_INFO("LB2 GPU sample {} VS={:016X} PS={:016X} internal={:016X} "
                "observed={} samples={} sampled_ms={:.3f} mean_us={:.3f} max_us={:.3f} mean_work={:.1f}",
                names[unsigned(key.kind)], key.vs, key.ps, key.internal_pipeline, stats.observed,
                stats.samples, stats.ns / 1e6, stats.ns / stats.samples / 1e3,
                stats.max_ns / 1e3, double(stats.work) / stats.samples);
  }
  if(!state.phase_mode)for (auto& [key, stats] : state.stats) stats = {};
  state.skipped = 0; state.report_time = std::chrono::steady_clock::now();
}

void Lb2GpuTimestampsShutdown(const Device* device) {
  if (state.device != device) return;
  if (!state.stats.empty()) Lb2GpuTimestampsReport();
  for (auto& page : state.pages) {
    device->functions().vkDestroyQueryPool(device->device(), page->pool, nullptr);
  }
  state = State{};
}
