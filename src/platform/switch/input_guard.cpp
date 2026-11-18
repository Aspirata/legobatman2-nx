#include <rex/hook.h>
#include <mutex>

namespace { std::recursive_mutex input_lock; }
// Static ELF imports use the linker's wrap mechanism. Keep the SDK's original
// marshaling and return values, including controller vibration.
#define LB2_SWITCH_INPUT(name) \
  REX_EXTERN(__real___imp__XamInput##name); \
  REX_HOOK_RAW(__wrap___imp__XamInput##name) { \
    std::lock_guard guard(input_lock); \
    __real___imp__XamInput##name(ctx,base); \
  }
LB2_SWITCH_INPUT(GetState)
LB2_SWITCH_INPUT(GetCapabilities)
LB2_SWITCH_INPUT(GetCapabilitiesEx)
LB2_SWITCH_INPUT(SetState)
LB2_SWITCH_INPUT(GetKeystroke)
LB2_SWITCH_INPUT(GetKeystrokeEx)
#undef LB2_SWITCH_INPUT
