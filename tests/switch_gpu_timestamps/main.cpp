#include "lb2_gpu_timestamps.h"
#include <rex/cvar.h>
#include <rex/ui/vulkan/device.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

std::vector<std::string> test_logs;
bool test_gpu_timestamps_enabled=true;
namespace {
unsigned checks=0, creates=0, destroys=0, result_reads=0, resets=0, writes=0;
bool ready=false, available=true, supported=true;
bool entrypoints=true;
unsigned valid_bits=8;
uint64_t tick=240;
struct Pool {std::vector<uint64_t> values;std::vector<bool> written;};
std::map<VkQueryPool,Pool> pools;
void Check(bool ok,const char* what) {
  ++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}
}
VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice,const VkQueryPoolCreateInfo* info,
                                     const VkAllocationCallbacks*,VkQueryPool* out) {
  Check(info->queryType==VK_QUERY_TYPE_TIMESTAMP,"Only timestamp pools are created");
  Check(info->queryCount<=128,"Each page is bounded");
  *out=reinterpret_cast<VkQueryPool>(uintptr_t(++creates+10));
  pools[*out].values.resize(info->queryCount);pools[*out].written.resize(info->queryCount);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL Destroy(VkDevice,VkQueryPool p,const VkAllocationCallbacks*) {
  Check(pools.erase(p)==1,"Each pool is destroyed exactly once");++destroys;
}
VKAPI_ATTR void VKAPI_CALL Reset(VkCommandBuffer,VkQueryPool p,uint32_t first,uint32_t n) {
  Check(pools.contains(p)&&first+n<=pools[p].values.size(),"Query reset stays in its page");++resets;
  for(unsigned i=first;i<first+n;++i)pools[p].written[i]=false;
}
VKAPI_ATTR void VKAPI_CALL Write(VkCommandBuffer,VkPipelineStageFlagBits stage,VkQueryPool p,uint32_t q) {
  Check(stage==VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT||stage==VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        "Timestamp stage is explicit");
  Check(pools.contains(p)&&q<pools[p].values.size(),"Query write stays in its page");
  pools[p].values[q]=valid_bits==64?tick:tick&((uint64_t{1}<<valid_bits)-1);
  pools[p].written[q]=true;tick+=20;++writes;
}
VKAPI_ATTR VkResult VKAPI_CALL Get(VkDevice,VkQueryPool p,uint32_t first,uint32_t count,
                                  size_t size,void* data,VkDeviceSize stride,VkQueryResultFlags flags) {
  ++result_reads;
  Check(!(flags&VK_QUERY_RESULT_WAIT_BIT),"Profiling never adds a blocking query wait");
  Check((flags&VK_QUERY_RESULT_64_BIT)&&(flags&VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
        "64-bit values and availability are requested together");
  Check(first==0&&count<=pools[p].values.size()&&size>=count*stride,"Only written slots are retrieved");
  for(unsigned i=0;i<count;++i)Check(pools[p].written[i],"Unwritten reset slots must not enter a read");
  if(!ready)return VK_NOT_READY;
  for(unsigned i=0;i<count;++i){uint64_t values[]{pools[p].values[i],uint64_t(available)};
    std::memcpy(static_cast<char*>(data)+i*stride,values,sizeof(values));}
  return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL Properties(VkPhysicalDevice,VkPhysicalDeviceProperties* p) {
  p->limits.timestampPeriod=2.f;
}
VKAPI_ATTR void VKAPI_CALL Families(VkPhysicalDevice,uint32_t* n,VkQueueFamilyProperties* p) {
  if(!p){*n=1;return;}*n=1;p[0].timestampValidBits=supported?valid_bits:0;
}
rex::ui::vulkan::VulkanDevice Device() {
  rex::ui::vulkan::VulkanDevice d;
  d.f={Create,Destroy,Reset};
  d.instance.f={Properties,Families,[](VkDevice,const char* name)->PFN_vkVoidFunction {
    if(!entrypoints)return nullptr;
    if(std::strcmp(name,"vkCmdWriteTimestamp")==0)return reinterpret_cast<PFN_vkVoidFunction>(Write);
    if(std::strcmp(name,"vkGetQueryPoolResults")==0)return reinterpret_cast<PFN_vkVoidFunction>(Get);
    return nullptr;
  }};return d;
}
}
int main() {
  auto d=Device();auto cmd=reinterpret_cast<VkCommandBuffer>(3);
  auto pipeline=reinterpret_cast<VkPipeline>(4);
  test_gpu_timestamps_enabled=false;
  Lb2GpuTimestampsLabel(pipeline,0xaaa,0xbbb);
  {Lb2GpuTimestampBatch batch(&d,cmd,1);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    auto token=batch.Begin(Lb2GpuTimestampKind::kDraw,12);batch.End(token);}
  Lb2GpuTimestampsCompleted(&d,1);
  Check(creates==0&&resets==0&&writes==0&&result_reads==0,
        "Startup-disabled recorder must add no query allocation, commands or result reads");
  Lb2GpuTimestampsShutdown(&d);test_gpu_timestamps_enabled=true;
  supported=false;
  {Lb2GpuTimestampBatch batch(&d,cmd,1);auto token=batch.Begin(Lb2GpuTimestampKind::kDraw,1);batch.End(token);}
  Check(creates==0&&writes==0,"Unsupported timestamps leave the command stream unchanged");
  Lb2GpuTimestampsShutdown(&d);supported=true;
  entrypoints=false;
  {Lb2GpuTimestampBatch batch(&d,cmd,1);}
  Check(creates==0&&writes==0,"Missing core entry points leave the command stream unchanged");
  Lb2GpuTimestampsShutdown(&d);entrypoints=true;
  Lb2GpuTimestampsLabel(pipeline,0x111,0x222);
  {Lb2GpuTimestampBatch batch(&d,cmd,7);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    auto token=batch.Begin(Lb2GpuTimestampKind::kDraw,12);batch.End(token);}
  Check(creates==1&&resets==1&&writes==4,"One sampled draw has two samples inside one batch span");
  Lb2GpuTimestampsLabel(pipeline,0x333,0x444);
  Lb2GpuTimestampsCompleted(&d,6);
  Check(result_reads==0,"Queries cannot be read before their submission fence completes");
  Lb2GpuTimestampsCompleted(&d,7);
  Check(result_reads==1,"Completed query reads are nonblocking");
  {Lb2GpuTimestampBatch batch(&d,cmd,8);}
  Check(creates==2,"A not-ready page is never reset or reused");
  ready=true;available=false;Lb2GpuTimestampsCompleted(&d,7);
  {Lb2GpuTimestampBatch batch(&d,cmd,9);}
  Check(creates==3,"Missing availability also prevents premature reuse");
  available=true;Lb2GpuTimestampsCompleted(&d,7);
  {Lb2GpuTimestampBatch batch(&d,cmd,10);}
  Check(creates==3,"A completed, available page may be safely reused");
  Lb2GpuTimestampsReport();
  bool label=false,duration=false;
  for(const auto& l:test_logs){label|=l.find("VS=0000000000000111 PS=0000000000000222")!=std::string::npos;
    duration|=l.find("mean_us=0.040")!=std::string::npos;}
  Check(label,"Sample output retains guest shader identity");
  Check(duration,"Valid-bit rollover and timestamp period produce correct duration");
  Lb2GpuTimestampsForget(pipeline);
  unsigned page_writes=writes, page_resets=resets;
  {Lb2GpuTimestampBatch batch(&d,cmd,12);
    for(unsigned i=0;i<200;++i){batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,
      reinterpret_cast<VkPipeline>(uintptr_t(1000+i)));
      auto t=batch.Begin(Lb2GpuTimestampKind::kIndexed,3);batch.End(t);}}
  Check(writes-page_writes==128&&resets-page_resets==1,
        "Full pages stop sampling without allocation/reset inside a render pass");
  Lb2GpuTimestampsCompleted(&d,12);
  // All remaining pages stay pending. Profiling must stop allocating at its
  // fixed budget, gracefully leaving subsequent command buffers unmodified.
  ready=false;
  for(unsigned i=0;i<80;++i){Lb2GpuTimestampBatch batch(&d,cmd,20+i);}
  Check(creates==64,"Query pool memory has a fixed upper bound");
  unsigned old_writes=writes;
  {Lb2GpuTimestampBatch batch(&d,cmd,200);auto t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);}
  Check(writes==old_writes,"Exhausted profiling budget never changes guest GPU work");
  Lb2GpuTimestampsShutdown(&d);
  Check(destroys==creates&&pools.empty(),"All pages are released at the existing idle shutdown");
  test_logs.clear();valid_bits=64;tick=UINT64_MAX-20;ready=true;
  {Lb2GpuTimestampBatch batch(&d,cmd,1);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    auto t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);}
  Lb2GpuTimestampsCompleted(&d,1);Lb2GpuTimestampsReport();
  bool wrap64=false,forgot=false;
  for(const auto& l:test_logs){wrap64|=l.find("mean_us=0.040")!=std::string::npos;
    forgot|=l.find("internal=0000000000000004")!=std::string::npos;}
  Check(wrap64,"64-bit rollover must not shift by64 or truncate ticks");
  Check(forgot,"A reused pipeline handle cannot inherit destroyed guest shader labels");
  {Lb2GpuTimestampBatch batch(&d,cmd,2);}
  Lb2GpuTimestampsAbandon();
  unsigned abandoned_writes=writes, abandoned_reads=result_reads;
  Lb2GpuTimestampsCompleted(&d,2);
  {Lb2GpuTimestampBatch batch(&d,cmd,3);}
  Check(writes==abandoned_writes&&result_reads==abandoned_reads,
        "Failed submissions disable sampling and cannot consume stale query values");
  Lb2GpuTimestampsShutdown(&d);
  Check(destroys==creates&&pools.empty(),"Reinitialized recorder releases its new page");
  test_gpu_timestamps_enabled=false;test_gpu_phase_profile_enabled=true;
  test_logs.clear();ready=true;available=true;valid_bits=64;
  const auto guest=reinterpret_cast<VkPipeline>(uintptr_t(9000));
  const auto transfer=reinterpret_cast<VkPipeline>(uintptr_t(9001));
  const auto unknown=reinterpret_cast<VkPipeline>(uintptr_t(9002));
  const auto direct_resolve=reinterpret_cast<VkPipeline>(uintptr_t(9003));
  Lb2GpuTimestampsLabel(guest,0xabc,0xdef);
  Lb2GpuTimestampsRole(transfer,"ownership_transfer");
  Lb2GpuTimestampsRole(unknown,"future_graphics_helper");
  Lb2GpuTimestampsRole(direct_resolve,"direct_color_rgba8");
  const auto before_writes=writes,before_reads=result_reads,before_resets=resets;
  {Lb2GpuTimestampBatch batch(&d,cmd,10);
    batch.RenderArea(960);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,guest);
    for(unsigned i=0;i<1000;++i){auto t=batch.Begin(Lb2GpuTimestampKind::kIndexed,100);batch.End(t);}
    batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,transfer);
    auto t=batch.Begin(Lb2GpuTimestampKind::kDraw,6);batch.End(t);
    batch.RenderArea(880);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,guest);
    t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);
    batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,unknown);
    t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);
    batch.Bind(VK_PIPELINE_BIND_POINT_COMPUTE,direct_resolve);
    t=batch.Begin(Lb2GpuTimestampKind::kDispatch,16);batch.End(t);
  }
  Check(writes-before_writes==12,"1004 operations become5 contiguous phase pairs plus buffer pair");
  Lb2GpuTimestampsCompleted(&d,13);
  Check(result_reads==before_reads,"Phase results wait for4 extra completed submissions");
  Lb2GpuTimestampsCompleted(&d,14);
  Check(result_reads==before_reads+1,"Delayed phase read uses completed available queries");
  Lb2GpuTimestampsReport();
  bool rt960=false,ownership=false,other=false,unknown_draw=false,direct=false;
  for(const auto& l:test_logs){
    rt960|=l.find("role=guest_rt960")!=std::string::npos&&l.find("sampled_operations=1000")!=std::string::npos;
    ownership|=l.find("role=ownership")!=std::string::npos;
    other|=l.find("role=guest_other")!=std::string::npos;
    unknown_draw|=l.find("role=other_draw")!=std::string::npos;
    direct|=l.find("role=resolve")!=std::string::npos;
  }
  Check(rt960&&ownership&&other,"GPU phases retain RT960, ownership and other rendering identities");
  Check(unknown_draw,"Unknown named graphics helpers retain draw classification");
  Check(direct,"Enabled direct-color resolve retains resolve classification");
  unsigned quiet=0,sampled=0;const auto phase_creates=creates;
  for(unsigned i=0;i<512;++i){auto before=writes;
    {Lb2GpuTimestampBatch batch(&d,cmd,20+i);
      batch.RenderArea(960);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,guest);
      auto t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);}
    if(writes==before)++quiet;else ++sampled;
  }
  Check(quiet>sampled*8 && sampled>0,"Random sparse selection leaves most command buffers unchanged");
  Check(creates-phase_creates<=7,"Phase mode never owns more than8 query pages");
  ready=false;auto no_ready_reads=result_reads;
  Lb2GpuTimestampsCompleted(&d,1000);auto first_poll=result_reads;
  Lb2GpuTimestampsCompleted(&d,1000);
  Check(first_poll>no_ready_reads&&result_reads==first_poll,"Same completed submission cannot busy-poll unavailable phase queries");
  ready=true;Lb2GpuTimestampsCompleted(&d,1001);
  for(unsigned i=2000;i<8000;++i){
    {Lb2GpuTimestampBatch batch(&d,cmd,i);auto t=batch.Begin(Lb2GpuTimestampKind::kCopy,16);batch.End(t);}
    Lb2GpuTimestampsCompleted(&d,i-4);
  }
  Check(resets-before_resets==64,"Phase capture has a64-buffer lifetime query budget");
  const auto capped_writes=writes;
  for(unsigned i=0;i<1024;++i){Lb2GpuTimestampBatch batch(&d,cmd,9000+i);}
  Check(writes==capped_writes,"Exhausted phase budget adds no GPU query commands");
  const auto swap=reinterpret_cast<VkPipeline>(uintptr_t(9004));
  Lb2GpuTimestampsRole(swap,"swap_gamma_graphics");
  {Lb2GpuTimestampBatch batch(&d,cmd,11000);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,swap);
    auto t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);}
  test_logs.clear();Lb2GpuTimestampsReport();
  bool unsampled=false;
  for(const auto& l:test_logs)unsampled|=l.find("role=swap observed_operations=1 samples=0 sampled_operations=0")!=std::string::npos;
  Check(unsampled,"Observed but unsampled roles must not disappear from coverage reports");
  Lb2GpuTimestampsShutdown(&d);
  Check(destroys==creates&&pools.empty(),"Phase pools release at existing idle shutdown");
  test_logs.clear();Lb2GpuTimestampsLabel(guest,0xabc,0xdef);
  const auto truncation_writes=writes;
  {Lb2GpuTimestampBatch batch(&d,cmd,10);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,guest);
    for(unsigned i=0;i<100;++i){batch.RenderArea((i&1)?848:960);
      auto t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);}}
  Check(writes-truncation_writes==128,"More than63 alternating phases stay within128 query writes");
  Lb2GpuTimestampsCompleted(&d,14);Lb2GpuTimestampsReport();
  bool truncated=false,coverage=false;
  for(const auto& l:test_logs){
    truncated|=l.find("truncated_segments=37")!=std::string::npos;
    coverage|=l.find("first_selected_submission=10 last_selected_submission=10 buffers_read=1")!=std::string::npos;
  }
  Check(truncated&&coverage,"Phase truncation and actual captured submission coverage are explicit");
  Lb2GpuTimestampsShutdown(&d);
  test_gpu_phase_coarse_enabled=true;test_logs.clear();
  Lb2GpuTimestampsLabel(guest,0xabc,0xdef);
  Lb2GpuTimestampsRole(transfer,"ownership_transfer");
  Lb2GpuTimestampsRole(direct_resolve,"texture_load");
  const auto coarse_writes=writes;
  {Lb2GpuTimestampBatch batch(&d,cmd,10);
    for(unsigned i=0;i<1000;++i){
      batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,(i&1)?transfer:unknown);
      auto t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);
    }
    batch.RenderArea(960);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,guest);
    for(unsigned i=0;i<1000;++i){
      auto t=batch.Begin(Lb2GpuTimestampKind::kIndexed,300);batch.End(t);
      batch.Bind(VK_PIPELINE_BIND_POINT_COMPUTE,direct_resolve);
      t=batch.Begin(Lb2GpuTimestampKind::kDispatch,1);batch.End(t);
      batch.RenderArea(480);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,transfer);
      t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);
      batch.RenderArea(960);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,guest);
    }
    batch.RenderArea(848);
    auto t=batch.Begin(Lb2GpuTimestampKind::kIndexed,3);batch.End(t);
    t=batch.Begin(Lb2GpuTimestampKind::kCopy,1);batch.End(t);
  }
  Check(writes-coarse_writes==8,"Coarse mode collapses1000 early changes and captures the entire late960 group");
  Lb2GpuTimestampsCompleted(&d,14);Lb2GpuTimestampsReport();
  bool late960=false,other_gpu=false,coarse_complete=false;
  for(const auto& l:test_logs){
    late960|=l.find("role=rt960_sequence observed_operations=3000 samples=1 sampled_operations=3000")!=std::string::npos;
    other_gpu|=l.find("role=other_gpu observed_operations=1002 samples=2 sampled_operations=1002")!=std::string::npos;
    coarse_complete|=l.find("selected_operations=1000 recorded_operations=1000")!=std::string::npos;
  }
  Check(late960&&other_gpu&&coarse_complete,"Late RT960 coverage and the combined other workload remain explicit");
  Lb2GpuTimestampsShutdown(&d);test_logs.clear();
  Lb2GpuTimestampsLabel(guest,0xabc,0xdef);
  {Lb2GpuTimestampBatch batch(&d,cmd,10);batch.Bind(VK_PIPELINE_BIND_POINT_GRAPHICS,guest);
    for(unsigned i=0;i<100;++i){batch.RenderArea((i&1)?848:960);
      auto t=batch.Begin(Lb2GpuTimestampKind::kDraw,3);batch.End(t);}}
  Lb2GpuTimestampsCompleted(&d,14);Lb2GpuTimestampsReport();
  bool incomplete960=false;
  for(const auto& l:test_logs)incomplete960|=l.find("role=guest_rt960 observed_operations=50 samples=0 sampled_operations=0 selected_operations=50 recorded_operations=32")!=std::string::npos;
  Check(incomplete960,"An overflowing coarse stream exposes exactly how much selected960 work was not recorded");
  Lb2GpuTimestampsShutdown(&d);test_gpu_phase_coarse_enabled=false;
  test_gpu_phase_profile_enabled=false;
  std::printf("PASS: %u GPU timestamp lifecycle, phase grouping, sampling, lag and nonblocking checks\n",checks);
}
