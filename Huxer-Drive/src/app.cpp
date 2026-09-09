#include "drive_ui.h"

using namespace huxerui;

namespace huxer_drive { View App(); bool DesktopWindow(); }

static AppOptions Options() {
  AppOptions options;
  options.window.title = "Huxer Drive";
  options.window.initial_size = {1280, 900};
  options.window.chrome_mode = huxer_drive::DesktopWindow() ? WindowChromeMode::Custom : WindowChromeMode::System;
  options.window.title_bar_height = 52;
  options.window.caption_labels = {.minimize = app::strings::window_minimize, .toggle_maximize = app::strings::window_maximize, .close = app::strings::close};
  options.viewport_breakpoints = {.medium_width = 720, .expanded_width = 1160};
  options.show_debug_overlay = false;
  return options;
}

const Application application{huxer_drive::App, Options()};
