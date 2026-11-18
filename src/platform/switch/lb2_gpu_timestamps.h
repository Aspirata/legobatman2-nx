#pragma once
#include <cstdint>
#include <rex/ui/vulkan/api.h>

namespace rex::ui::vulkan { class VulkanDevice; }

enum class Lb2GpuTimestampKind : unsigned { kDraw, kIndexed, kDispatch, kCopy, kCommandBuffer, kPhase };

// All operations run on the command processor thread. Query pages are reset
// outside render passes and only reused after a completed submission and read.
class Lb2GpuTimestampBatch {
 public:
  Lb2GpuTimestampBatch(const rex::ui::vulkan::VulkanDevice*, VkCommandBuffer, uint64_t submission);
  ~Lb2GpuTimestampBatch();
  Lb2GpuTimestampBatch(const Lb2GpuTimestampBatch&) = delete;
  Lb2GpuTimestampBatch& operator=(const Lb2GpuTimestampBatch&) = delete;
  void Bind(VkPipelineBindPoint, VkPipeline);
  void RenderArea(uint32_t width) { render_width_=width; }
  unsigned Begin(Lb2GpuTimestampKind, uint64_t work);
  void End(unsigned token);
 private:
  void* page_ = nullptr;
  VkCommandBuffer command_;
  VkPipeline graphics_ = VK_NULL_HANDLE, compute_ = VK_NULL_HANDLE;
  uint32_t render_width_=0;
  unsigned phase_token_=UINT32_MAX,phase_role_=UINT32_MAX;
  bool phase_enabled_=false;
  bool in_rt960_sequence_=false;
  void Phase(Lb2GpuTimestampKind,uint64_t work);
  void EndPhase();
};

class Lb2GpuTimestampSample {
 public:
  Lb2GpuTimestampSample(Lb2GpuTimestampBatch& batch, Lb2GpuTimestampKind kind, uint64_t work)
      : batch_(batch), token_(batch.Begin(kind, work)) {}
  ~Lb2GpuTimestampSample() { batch_.End(token_); }
 private:
  Lb2GpuTimestampBatch& batch_;
  unsigned token_;
};

void Lb2GpuTimestampsLabel(VkPipeline, uint64_t vertex_shader, uint64_t pixel_shader);
void Lb2GpuTimestampsRole(VkPipeline,const char* role);
void Lb2GpuTimestampsForget(VkPipeline);
// A recorded buffer that was never accepted cannot produce trustworthy data.
// Keep its resources until shutdown; do not read or reuse any pending pages.
void Lb2GpuTimestampsAbandon();
void Lb2GpuTimestampsCompleted(const rex::ui::vulkan::VulkanDevice*, uint64_t completed);
// Caller has already used the existing queue-idle shutdown path.
void Lb2GpuTimestampsShutdown(const rex::ui::vulkan::VulkanDevice*);
void Lb2GpuTimestampsReport();

// Optional creation-time metadata only; callable by title helpers in ordinary
// builds where the diagnostic recorder is not linked. All calls use the CP thread.
extern "C" void Lb2GpuTimestampsPipelineName(VkPipeline, const char* role,
                                             uint64_t key, uint64_t variant)
    __attribute__((weak));
