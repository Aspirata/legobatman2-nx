#pragma once
#include "lb2_direct_color_rules.h"
#include <rex/ui/vulkan/api.h>
#include <rex/graphics/vulkan/command_processor.h>
bool Lb2PrepareDirectColorResolve(rex::graphics::vulkan::VulkanCommandProcessor&,
                                 VkDescriptorSetLayout source_layout);
void Lb2RecordDirectColorResolve(rex::graphics::vulkan::VulkanCommandProcessor&,
    VkDescriptorSet source,VkDescriptorSet destination,const Lb2DirectColorPush&);
void Lb2ShutdownDirectColorResolve(const rex::ui::vulkan::VulkanDevice*);
extern "C" void Lb2DirectColorAttempt(bool eligible);
extern "C" void Lb2DirectColorReport();
