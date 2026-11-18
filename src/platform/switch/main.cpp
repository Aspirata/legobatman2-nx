#include "legobatman2_init.h"
#include "game_build.h"
#include "game_identity.h"
#include <rex/rex_app.h>
#include <rex/audio/flags.h>
#include <rex/chrono/clock.h>
#include <rex/system/gpu_plugin.h>
#include <stdexcept>
#include <cstdlib>

class LegoBatman2SwitchApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<LegoBatman2SwitchApp>(new LegoBatman2SwitchApp(ctx,"legobatman2",PPCImageConfig));
  }
 protected:
  void OnConfigurePaths(rex::PathConfig& paths) override {
    // libnx resolves rooted paths on the default SD device. libstdc++ uses
    // POSIX rules, so absolute("sdmc:/...") wrongly prepends the NRO directory.
    paths.game_data_root="/switch/legobatman2/game";
    paths.user_data_root="/switch/legobatman2/userdata";
    paths.cache_root="/switch/legobatman2/cache";
    paths.metadata_root="/switch/legobatman2/metadata";
    paths.config_path="/switch/legobatman2/legobatman2.toml";
  }
  void OnPreSetup(rex::RuntimeConfig& config) override {
    // Existing driver CPU counters only; no GPU queries or ordering changes.
    if (::setenv("NVK_SWITCH_PERF_LOG", "1", 1) != 0)
      REXLOG_WARN("LB2 native queue counters could not be enabled");
    config.gpu_plugin="xenos";
    config.graphics=rex::system::LoadGpuPlugin(config.gpu_plugin,"vulkan");
    if(!config.graphics) throw std::runtime_error("LB2 Switch GPU initialization failed");
    // Keep the proven current memexport readback and exact extent estimation.
    for(const auto& setting : {std::pair{"readback_memexport_fast","false"},
                               std::pair{"execute_unclipped_draw_vs_on_cpu","true"}}) {
      if(!rex::cvar::SetFlagByName(setting.first,setting.second))
        throw std::runtime_error("LB2 Switch GPU compatibility setting unavailable");
    }
    REXCVAR_SET(audio_mute,false);
  }
  void OnPostSetup() override { rex::chrono::Clock::set_guest_time_scalar(1.0); }
  bool ConstructRuntime(const rex::PathConfig& paths) override {
    if(!paths.update_data_root.empty() || !lb2::verify_game(paths.game_data_root/"Default.xex",LB2_XEX_SHA256)) {
      REXLOG_ERROR("LB2 requires the selected base-disc Default.xex without title updates");
      return false;
    }
    return rex::ReXApp::ConstructRuntime(paths);
  }
  void OnLoadXexImage(std::string& image) override { image="game:\\Default.xex"; }
};
REX_DEFINE_APP(legobatman2,LegoBatman2SwitchApp::Create)
