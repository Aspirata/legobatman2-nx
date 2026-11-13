#include "legobatman2_init.h"
#include "game_build.h"
#include "game_identity.h"
#include <rex/rex_app.h>

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
  void OnPreSetup(rex::RuntimeConfig& config) override { config.gpu_plugin = "xenos"; }
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
