#include "lb2_stencil_copy.h"
#include "lb2_stencil_copy_shader.h"
#include <rex/logging.h>
#include <rex/ui/vulkan/util.h>

namespace {
struct State {
  VkDevice device=VK_NULL_HANDLE;
  VkPipelineLayout layout=VK_NULL_HANDLE;
  VkPipeline pipeline=VK_NULL_HANDLE;
  bool attempted=false;
};
State state;
bool PreparePipeline(rex::graphics::vulkan::VulkanCommandProcessor& processor,
                      VkDescriptorSetLayout source_layout) {
  auto* device=processor.GetVulkanDevice();
  if(state.attempted)return state.device==device->device()&&state.pipeline!=VK_NULL_HANDLE;
  state.attempted=true;state.device=device->device();
  const auto& f=device->functions();
  VkDescriptorSetLayout layouts[]={source_layout,processor.GetSingleTransientDescriptorLayout(
      rex::graphics::vulkan::VulkanCommandProcessor::SingleTransientDescriptorLayout::kStorageBufferCompute)};
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(lb2::stencil::Push)};
  VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layout.setLayoutCount=2;layout.pSetLayouts=layouts;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;
  if(f.vkCreatePipelineLayout(state.device,&layout,nullptr,&state.layout)!=VK_SUCCESS)return false;
  auto code=Lb2BuildStencilCopyShader();
  VkShaderModule module=rex::ui::vulkan::util::CreateShaderModule(device,code.data(),code.size()*4);
  if(module==VK_NULL_HANDLE)return false;
  VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  info.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;info.stage.module=module;info.stage.pName="main";
  info.layout=state.layout;
  info.basePipelineIndex=-1;
  VkResult result=f.vkCreateComputePipelines(state.device,VK_NULL_HANDLE,1,&info,nullptr,&state.pipeline);
  f.vkDestroyShaderModule(state.device,module,nullptr);
  if(result!=VK_SUCCESS){
    if(state.pipeline)f.vkDestroyPipeline(state.device,state.pipeline,nullptr);
    state.pipeline=VK_NULL_HANDLE;return false;
  }
  REXLOG_INFO("LB2 stencil copy: compute4x-to1x ready; only matching depth base/pitch/format at scale1");
  return true;
}
}

bool Lb2PrepareStencilCopy(rex::graphics::vulkan::VulkanCommandProcessor& processor,
                          VkDescriptorSetLayout source_layout,VkDeviceSize bytes,
                          Lb2StencilCopyRecording& recording) {
  if(!bytes||bytes>4*1024*1024||bytes>processor.GetVulkanDevice()->properties().maxStorageBufferRange||
     !PreparePipeline(processor,source_layout))return false;
  using Layout=rex::graphics::vulkan::VulkanCommandProcessor::SingleTransientDescriptorLayout;
  auto descriptor=processor.AllocateSingleTransientDescriptor(Layout::kStorageBufferCompute);
  if(descriptor==VK_NULL_HANDLE)return false;
  auto scratch=processor.AcquireScratchGpuBuffer(bytes,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT);
  if(scratch.buffer()==VK_NULL_HANDLE)return false;
  VkDescriptorBufferInfo buffer{scratch.buffer(),0,bytes};
  VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstSet=descriptor;write.dstBinding=0;write.descriptorCount=1;
  write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&buffer;
  auto* device=processor.GetVulkanDevice();device->functions().vkUpdateDescriptorSets(device->device(),1,&write,0,nullptr);
  recording.scratch=std::move(scratch);recording.storage_set=descriptor;recording.range=bytes;
  return true;
}

void Lb2RecordStencilCopy(rex::graphics::vulkan::VulkanCommandProcessor& processor,
                         Lb2StencilCopyRecording& recording,VkDescriptorSet source,
                         VkImage destination,const lb2::stencil::Plan& plan) {
  auto& commands=processor.deferred_command_buffer();
  processor.PushBufferMemoryBarrier(recording.scratch.buffer(),0,recording.range,
      recording.scratch.GetStageMask(),VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      recording.scratch.GetAccessMask(),VK_ACCESS_SHADER_WRITE_BIT);
  processor.SubmitBarriers(true);
  processor.BindExternalComputePipeline(state.pipeline);
  VkDescriptorSet sets[]={source,recording.storage_set};
  commands.CmdVkBindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE,state.layout,0,2,sets,0,nullptr);
  commands.CmdVkPushConstants(state.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(plan.push),&plan.push);
  commands.CmdVkDispatch(plan.groups,1,1);
  processor.PushBufferMemoryBarrier(recording.scratch.buffer(),0,recording.range,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
  processor.SubmitBarriers(true);
  VkBufferImageCopy region{};
  region.bufferRowLength=plan.push.row_words*4;
  region.imageSubresource={VK_IMAGE_ASPECT_STENCIL_BIT,0,0,1};
  region.imageOffset={int32_t(plan.push.x),int32_t(plan.push.y),0};
  region.imageExtent={plan.push.width,plan.push.height,1};
  commands.CmdVkCopyBufferToImage(recording.scratch.buffer(),destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
  recording.scratch.SetStageMask(VK_PIPELINE_STAGE_TRANSFER_BIT);
  recording.scratch.SetAccessMask(VK_ACCESS_TRANSFER_READ_BIT);
}

void Lb2ShutdownStencilCopy(const rex::ui::vulkan::VulkanDevice* device) {
  if(state.device!=device->device())return;
  // The command processor's original shutdown waits for all queue operations
  // before destroying its render-target cache. No new wait is introduced here.
  const auto& f=device->functions();
  if(state.pipeline)f.vkDestroyPipeline(state.device,state.pipeline,nullptr);
  if(state.layout)f.vkDestroyPipelineLayout(state.device,state.layout,nullptr);
  state={};
}
