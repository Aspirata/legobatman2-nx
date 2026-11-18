#pragma once
#include <vulkan/vulkan.h>

namespace rex::ui::vulkan {
struct VulkanInstance {
  struct Functions {
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
  } f;
  const Functions& functions() const { return f; }
};
struct VulkanDevice {
  struct Functions {
    PFN_vkCreateQueryPool vkCreateQueryPool;
    PFN_vkDestroyQueryPool vkDestroyQueryPool;
    PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
  } f;
  VulkanInstance instance;
  const Functions& functions() const { return f; }
  const VulkanInstance* vulkan_instance() const { return &instance; }
  VkDevice device() const { return reinterpret_cast<VkDevice>(1); }
  VkPhysicalDevice physical_device() const { return reinterpret_cast<VkPhysicalDevice>(2); }
  uint32_t queue_family_graphics_compute() const { return 0; }
};
}
