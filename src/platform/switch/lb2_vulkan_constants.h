#pragma once
#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace lb2 {
// NVK can bind dynamic UBO addresses directly on Maxwell. Ordinary UBOs
// require indirect descriptor reads, MME calls and GPFIFO sync boundaries.
inline constexpr VkDescriptorType kConstantDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

inline std::array<VkDescriptorPoolSize, 2> UniformPoolSizes(VkDescriptorPoolSize ordinary) {
  // This allocator also serves ordinary transient compute UBOs.
  return {ordinary, VkDescriptorPoolSize{kConstantDescriptorType, ordinary.descriptorCount}};
}

constexpr uint32_t ConstantDynamicOffsetCount(uint32_t first_set, uint32_t set_count,
                                            uint32_t constant_set, uint32_t constant_count) {
  return constant_set >= first_set && constant_set - first_set < set_count ? constant_count : 0;
}
}
