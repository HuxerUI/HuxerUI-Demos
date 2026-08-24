#include <memory>

#include <app_resources.h>
#include <huxerui/huxerui.h>

#include "mail_app.h"
#include "mail_platform.h"
#include "mock_mail_service.h"

using namespace huxerui;

namespace {

AppOptions BuildOptions() {
  AppOptions options;
  options.window.title = "Huxer Mail";
  options.window.initial_size = {1180.0F, 720.0F};
  options.window.content_mode = WindowContentMode::SafeArea;
  if constexpr (huxer_mail::kUsesDesktopWindowChrome) {
    options.window.chrome_mode = WindowChromeMode::Custom;
    options.window.title_bar_height = 38.0F;
    options.window.caption_labels = WindowCaptionLabels{
        .minimize = app::strings::window_minimize,
        .toggle_maximize = app::strings::window_toggle_maximize,
        .close = app::strings::window_close,
    };
  } else {
    options.window.chrome_mode = WindowChromeMode::System;
  }
  options.viewport_breakpoints = {.medium_width = 720.0F, .expanded_width = 1120.0F};
  options.show_debug_overlay = false;
  options.root_hooks.push_back([](RootContext& root) {
    root.Provide(std::make_shared<huxer_mail::MockMailService>());
  });
  return options;
}

} // namespace

const Application application{huxer_mail::HuxerMailApp, BuildOptions()};
