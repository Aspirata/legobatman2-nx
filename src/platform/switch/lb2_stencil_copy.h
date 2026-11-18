#pragma once
#include "lb2_stencil_copy_rules.h"
#include <rex/ui/vulkan/api.h>
#include <rex/graphics/vulkan/command_processor.h>

struct Lb2StencilCopyRecording {
  rex::graphics::vulkan::VulkanCommandProcessor::ScratchBufferAcquisition scratch;
  VkDescriptorSet storage_set=VK_NULL_HANDLE;
  VkDeviceSize range=0;
};
bool Lb2PrepareStencilCopy(rex::graphics::vulkan::VulkanCommandProcessor& processor,
                          VkDescriptorSetLayout source_layout,VkDeviceSize bytes,
                          Lb2StencilCopyRecording& recording);
void Lb2RecordStencilCopy(rex::graphics::vulkan::VulkanCommandProcessor& processor,
                         Lb2StencilCopyRecording& recording,VkDescriptorSet source,
                         VkImage destination,const lb2::stencil::Plan& plan);
void Lb2ShutdownStencilCopy(const rex::ui::vulkan::VulkanDevice* device);
