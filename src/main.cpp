#include "legobatman2_init.h"
#include "game_build.h"
#include "game_identity.h"
#include "xma_packet_fix.h"
#include <rex/rex_app.h>
#include <rex/audio/flags.h>
#include <rex/chrono/clock.h>
#include <rex/system/gpu_plugin.h>
#include <stdexcept>

class LegoBatman2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<LegoBatman2App>(new LegoBatman2App(ctx, "legobatman2", PPCImageConfig));
  }
 protected:
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (paths.game_data_root.empty()) paths.game_data_root = std::filesystem::current_path() / "game";
  }
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (!lb2::install_xma_packet_fix()) {
      throw std::runtime_error("Unsupported SDK runtime: cannot apply LB2 XMA compatibility fix");
    }
    config.gpu_plugin = "xenos";
    // Load first so plugin-owned settings and deferred command-line values
    // are registered before choosing the title default.
    config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin);
    // LB2 clears a 640x360 4x-MSAA depth surface at EDRAM tile 1328.
    // Estimating this unclipped rectangle as 1024 rows claims all 2048 tiles,
    // including the live color buffer at tile 0. Estimate the vertex extent
    // precisely so RTV ownership stays within the actual 720 depth tiles.
    if (rex::cvar::GetFlagSource("execute_unclipped_draw_vs_on_cpu") != rex::cvar::Source::kCommandLine) {
      if (!rex::cvar::SetFlagByName("execute_unclipped_draw_vs_on_cpu", "true")) {
        REXLOG_ERROR("Could not enable precise LB2 render target extent estimation");
      }
    }
    // LB2 consumes current memexport data. The delayed fast path reuses a
    // previous operation's output and corrupts animated scene geometry.
    if (rex::cvar::GetFlagSource("readback_memexport_fast") != rex::cvar::Source::kCommandLine) {
      if (!rex::cvar::SetFlagByName("readback_memexport_fast", "false")) {
        REXLOG_ERROR("Could not enable current LB2 memexport readback");
      }
    }
    if (rex::cvar::GetFlagSource("render_target_path_d3d12") != rex::cvar::Source::kCommandLine) {
      if (!rex::cvar::SetFlagByName("render_target_path_d3d12", "rtv")) {
        REXLOG_ERROR("Could not select the LB2 RTV render target path");
      }
    }
    REXLOG_INFO("LB2 D3D12 render target path: {}; precise unclipped draw extent: {}; fast memexport readback: {}",
                rex::cvar::GetFlagByName("render_target_path_d3d12"),
                rex::cvar::GetFlagByName("execute_unclipped_draw_vs_on_cpu"),
                rex::cvar::GetFlagByName("readback_memexport_fast"));
    // Normal play uses sound; retain an explicit command-line mute override.
    if (rex::cvar::GetFlagSource("audio_mute") != rex::cvar::Source::kCommandLine) {
      REXCVAR_SET(audio_mute, false);
    }
  }
  void OnPostSetup() override {
    rex::chrono::Clock::set_guest_time_scalar(1.0);
    REXLOG_INFO("LB2 normal play: speed=1x, audio_mute={}, XMA compatibility fix enabled, input polling serialized",
                REXCVAR_GET(audio_mute));
  }
  bool ConstructRuntime(const rex::PathConfig& paths) override {
    if (!paths.update_data_root.empty()) {
      REXLOG_ERROR("Title updates are not supported by this base-disc build");
      return false;
    }
    if (!lb2::verify_game(paths.game_data_root / "Default.xex", LB2_XEX_SHA256)) {
      REXLOG_ERROR("Selected Default.xex is missing, unreadable or has the wrong SHA-256");
      return false;
    }
    REXLOG_INFO("Selected LB2 XEX hash verified; constructing runtime");
    return rex::ReXApp::ConstructRuntime(paths);
  }
  void OnLoadXexImage(std::string& image) override { image = "game:\\Default.xex"; }
  void OnPreLaunchModule() override { REXLOG_INFO("Launching LB2 guest entry 0x82B577E0"); }
};
REX_DEFINE_APP(legobatman2, LegoBatman2App::Create)
