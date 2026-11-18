#include "legobatman2_pch.h"
#include <rex/hook.h>
#include <rex/kernel/xboxkrnl/video.h>
#include <atomic>

// Addresses and call sites are from the hash-verified LB2 base-disc image.
// Keep the original routines for all allocation, formatting and ABI behavior.
REX_EXTERN(__imp__sub_824B0D70);
REX_EXTERN(__imp__sub_824B0C90);
REX_EXTERN(__imp__sub_824B0B08);
REX_EXTERN(__imp__sub_824B0B70);
REX_EXTERN(__real___imp__VdSwap);
extern "C" void RexSwitchPerfResolution(unsigned width, unsigned height);
extern "C" void Lb2GpuProfileFrame();

namespace {
struct Mode { uint32_t width, height; };
const Mode& configured_mode() {
  // Use the SDK's effective mode, including bounds and resolution presets.
  static const Mode mode = [] {
    rex::system::X_VIDEO_MODE video_mode;
    rex::kernel::xboxkrnl::VdQueryVideoMode(&video_mode);
    return Mode{uint32_t(video_mode.display_width), uint32_t(video_mode.display_height)};
  }();
  return mode;
}
void restore_mode(uint32_t output, uint8_t* base) {
  const auto& mode = configured_mode();
  REX_STORE_U32(output, mode.width);
  REX_STORE_U32(output + 4, mode.height);
  REX_STORE_U32(output + 16, mode.width >= 1280 || mode.height >= 720);
  RexSwitchPerfResolution(mode.width, mode.height);
  static std::atomic<unsigned> logs{0};
  if (logs.fetch_add(1, std::memory_order_relaxed) < 8)
    REXLOG_INFO("LB2 internal video mode: {}x{} (game 720p override removed)", mode.width, mode.height);
}
void resize_scene_surface(PPCContext& ctx) {
  // Only the five full-screen color/depth allocations in sub_824167D0.
  // Shadow maps, half-size depth, split-screen targets and other callers keep
  // their original dimensions and EDRAM layout.
  switch (ctx.lr) {
    case 0x824171FC: case 0x82417228: case 0x82417264:
    case 0x824172A0: case 0x824172D0: break;
    default: return;
  }
  if (ctx.r3.u32 != 1280 || ctx.r4.u32 != 720) return;
  const auto& mode = configured_mode();
  ctx.r3.u64 = mode.width;
  ctx.r4.u64 = mode.height;
  REXLOG_INFO("LB2 scene surface at {:08X}: 1280x720 -> {}x{}", ctx.lr, mode.width, mode.height);
}
}

REX_HOOK_RAW(sub_824B0D70) {
  const uint32_t output = ctx.r3.u32;
  __imp__sub_824B0D70(ctx, base);
  restore_mode(output, base);
}
REX_HOOK_RAW(sub_824B0C90) {
  const uint32_t output = ctx.r3.u32;
  __imp__sub_824B0C90(ctx, base);
  restore_mode(output, base);
}
REX_HOOK_RAW(sub_824B0B08) {
  resize_scene_surface(ctx);
  __imp__sub_824B0B08(ctx, base);
}
REX_HOOK_RAW(sub_824B0B70) {
  resize_scene_surface(ctx);
  __imp__sub_824B0B70(ctx, base);
}

// VdSwap arguments 9/10 are guest pointers, marshaled from stack slots by the
// SDK. Log the actual submitted frame size, independently of the display mode.
REX_HOOK_RAW(__wrap___imp__VdSwap) {
  Lb2GpuProfileFrame();
  static std::atomic<unsigned> frames{0};
  const unsigned frame = frames.fetch_add(1, std::memory_order_relaxed);
  if (frame < 5 || frame % 300 == 0) {
    const uint32_t width_ptr = REX_LOAD_U32(ctx.r1.u32 + 0x54);
    const uint32_t height_ptr = REX_LOAD_U32(ctx.r1.u32 + 0x5C);
    REXLOG_INFO("LB2 submitted frame {}: {}x{}", frame,
                REX_LOAD_U32(width_ptr), REX_LOAD_U32(height_ptr));
  }
  __real___imp__VdSwap(ctx, base);
}
