#include <rex/hook.h>
#include <windows.h>
#include <mutex>
#include <stdexcept>

namespace {
// SDK InputSystem::RefreshDevices mutates shared device vectors in every
// getter. Serialize the guest entry points while preserving SDK marshaling,
// device assignment, keyboard/controller support and vibration.
std::recursive_mutex input_lock;
PPCFunc* original_input(const char* name) {
  const auto module = GetModuleHandleW(L"rexruntime.dll");
  auto* fn = module ? reinterpret_cast<PPCFunc*>(GetProcAddress(module, name)) : nullptr;
  if (!fn) throw std::runtime_error("Pinned SDK input export missing");
  return fn;
}
}

#define LB2_GUARD_INPUT(name) \
  REX_HOOK_RAW(name) { \
    static auto* original = original_input(#name); \
    std::lock_guard guard(input_lock); \
    original(ctx, base); \
  }
LB2_GUARD_INPUT(__imp__XamInputGetState)
LB2_GUARD_INPUT(__imp__XamInputGetCapabilities)
LB2_GUARD_INPUT(__imp__XamInputGetCapabilitiesEx)
LB2_GUARD_INPUT(__imp__XamInputSetState)
LB2_GUARD_INPUT(__imp__XamInputGetKeystroke)
LB2_GUARD_INPUT(__imp__XamInputGetKeystrokeEx)
#undef LB2_GUARD_INPUT
