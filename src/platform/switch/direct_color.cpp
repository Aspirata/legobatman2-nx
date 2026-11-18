#include "lb2_direct_color.h"
#include "lb2_gpu_timestamps.h"
#include <rex/logging.h>
#include <rex/ui/vulkan/util.h>
#include <atomic>
namespace {
struct State {
  VkDevice device=VK_NULL_HANDLE;
  VkPipelineLayout layout=VK_NULL_HANDLE;
  VkPipeline pipeline=VK_NULL_HANDLE;
  bool attempted=false;
} state;
std::atomic<uint64_t> attempts{0},eligible{0},dispatches{0},pixels{0};
}
bool Lb2PrepareDirectColorResolve(rex::graphics::vulkan::VulkanCommandProcessor& cp,
                                 VkDescriptorSetLayout source_layout) {
  auto* device=cp.GetVulkanDevice();
  if(state.attempted)return state.device==device->device()&&state.pipeline!=VK_NULL_HANDLE;
  state.attempted=true;state.device=device->device();const auto& f=device->functions();
  VkDescriptorSetLayout layouts[]={source_layout,cp.GetSingleTransientDescriptorLayout(
      rex::graphics::vulkan::VulkanCommandProcessor::SingleTransientDescriptorLayout::kStorageBufferCompute)};
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Lb2DirectColorPush)};
  VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layout.setLayoutCount=2;layout.pSetLayouts=layouts;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;
  if(f.vkCreatePipelineLayout(state.device,&layout,nullptr,&state.layout)!=VK_SUCCESS)return false;
  const auto code=Lb2BuildDirectColorResolveShader();
  const auto module=rex::ui::vulkan::util::CreateShaderModule(device,code.data(),code.size()*4);
  if(module==VK_NULL_HANDLE)return false;
  VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  info.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;info.stage.module=module;info.stage.pName="main";
  info.layout=state.layout;info.basePipelineIndex=-1;
  const auto result=f.vkCreateComputePipelines(state.device,VK_NULL_HANDLE,1,&info,nullptr,&state.pipeline);
  f.vkDestroyShaderModule(state.device,module,nullptr);
  if(result!=VK_SUCCESS){if(state.pipeline)f.vkDestroyPipeline(state.device,state.pipeline,nullptr);state.pipeline=VK_NULL_HANDLE;return false;}
  if(Lb2GpuTimestampsPipelineName)
    Lb2GpuTimestampsPipelineName(state.pipeline,"direct_color_rgba8",0,0);
  REXLOG_INFO("LB2 direct color resolve:1x RGBA8 ready; full same-owner scale1 only");
  return true;
}
void Lb2RecordDirectColorResolve(rex::graphics::vulkan::VulkanCommandProcessor& cp,
    VkDescriptorSet source,VkDescriptorSet destination,const Lb2DirectColorPush& push) {
  cp.BindExternalComputePipeline(state.pipeline);
  auto& commands=cp.deferred_command_buffer();
  const VkDescriptorSet sets[]={source,destination};
  commands.CmdVkBindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE,state.layout,0,2,sets,0,nullptr);
  commands.CmdVkPushConstants(state.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),&push);
  cp.SubmitBarriers(true);
  commands.CmdVkDispatch((push.width+7)/8,(push.height+7)/8,1);
  dispatches.fetch_add(1,std::memory_order_relaxed);
  pixels.fetch_add(uint64_t(push.width)*push.height,std::memory_order_relaxed);
}
void Lb2ShutdownDirectColorResolve(const rex::ui::vulkan::VulkanDevice* device) {
  if(state.device!=device->device())return;
  const auto& f=device->functions();
  if(state.pipeline)f.vkDestroyPipeline(state.device,state.pipeline,nullptr);
  if(state.layout)f.vkDestroyPipelineLayout(state.device,state.layout,nullptr);
  state={};
}
extern "C" void Lb2DirectColorAttempt(bool accepted) {
  attempts.fetch_add(1,std::memory_order_relaxed);
  if(accepted)eligible.fetch_add(1,std::memory_order_relaxed);
}
extern "C" void Lb2DirectColorReport() {
  REXLOG_INFO("LB2 direct color resolve: attempts={} eligible={} dispatches={} pixels={}; "
              "fused source-image to guest buffer, original fallback/clears retained",
              attempts.exchange(0,std::memory_order_relaxed),eligible.exchange(0,std::memory_order_relaxed),
              dispatches.exchange(0,std::memory_order_relaxed),pixels.exchange(0,std::memory_order_relaxed));
}
