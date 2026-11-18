#pragma once
extern bool test_gpu_timestamps_enabled;
inline bool test_gpu_phase_profile_enabled=false;
inline bool test_gpu_phase_coarse_enabled=false;
#include <string_view>
inline bool TestTimestampFlag(std::string_view name) {
  if(name=="lb2_gpu_phase_profile")return test_gpu_phase_profile_enabled;
  if(name=="lb2_gpu_phase_coarse")return test_gpu_phase_coarse_enabled;
  return test_gpu_timestamps_enabled;
}
namespace rex::cvar { enum class Lifecycle { kInitOnly }; }
struct TimestampCvarRegistration {
  constexpr TimestampCvarRegistration lifecycle(rex::cvar::Lifecycle) const { return {}; }
};
#define REXCVAR_DEFINE_BOOL(name, value, category, description) \
  [[maybe_unused]] static auto timestamp_registration_##name = TimestampCvarRegistration{}
#define REXCVAR_GET(name) TestTimestampFlag(#name)
