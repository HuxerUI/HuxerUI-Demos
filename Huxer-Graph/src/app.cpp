#include "graph_ui.h"
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace graph::ui {
bool Desktop() {
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
  return false;
#else
  return true;
#endif
}
static ThemeSpec GraphTheme(bool dark) {
  auto t = dark ? MaterialDarkThemeSpec() : MaterialLightThemeSpec();
  t.colors.primary = dark ? Color::Rgb(169, 180, 236) : Color::Rgb(78, 94, 153);
  t.colors.on_primary = dark ? Color::Rgb(25, 34, 65) : Color::White();
  t.colors.primary_container = dark ? Color::Rgb(47, 57, 86) : Color::Rgb(231, 235, 248);
  t.colors.on_primary_container = dark ? Color::Rgb(220, 226, 255) : Color::Rgb(47, 62, 115);
  t.colors.secondary = dark ? Color::Rgb(182, 190, 206) : Color::Rgb(89, 102, 122);
  t.colors.on_secondary = dark ? Color::Rgb(31, 38, 51) : Color::White();
  t.colors.surface = dark ? Color::Rgb(22, 26, 34) : Color::Rgb(253, 254, 255);
  t.colors.background = t.colors.surface;
  t.colors.surface_container_low = dark ? Color::Rgb(27, 32, 41) : Color::Rgb(246, 248, 251);
  t.colors.surface_container = dark ? Color::Rgb(34, 40, 51) : Color::Rgb(239, 242, 246);
  t.colors.surface_container_high = dark ? Color::Rgb(41, 48, 60) : Color::Rgb(232, 236, 242);
  t.colors.surface_container_highest = dark ? Color::Rgb(48, 56, 69) : Color::Rgb(225, 231, 239);
  t.colors.secondary_container = dark ? Color::Rgb(43, 51, 67) : Color::Rgb(232, 237, 245);
  t.colors.on_secondary_container = dark ? Color::Rgb(214, 222, 238) : Color::Rgb(58, 72, 97);
  t.colors.tertiary_container = dark ? Color::Rgb(37, 64, 64) : Color::Rgb(224, 241, 237);
  t.colors.on_tertiary_container = dark ? Color::Rgb(184, 223, 215) : Color::Rgb(39, 88, 79);
  t.colors.on_surface = dark ? Color::Rgb(229, 233, 240) : Color::Rgb(35, 46, 64);
  t.colors.on_surface_variant = dark ? Color::Rgb(170, 182, 200) : Color::Rgb(95, 108, 128);
  t.colors.outline = dark ? Color::Rgb(66, 77, 94) : Color::Rgb(207, 215, 226);
  return t;
}
[[huxerui::composable]]
static View GraphControlStyles(View content) {
  const auto& t = UseTheme();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  auto button = UseEnvironment<ButtonStyle>();
  button.background = t.colors.secondary_container;
  button.label_style = {Font::System(14).WithWeight(FontWeight::Medium), t.colors.on_secondary_container};
  button.minimum_height = compact ? 44 : 32;
  button.padding = EdgeInsets::Symmetric(compact ? 16 : 12, compact ? 10 : 6);
  ThemeDefinition styles; styles.Set(button);
  if (!compact) {
    auto select = UseEnvironment<SelectStyle>();
    select.minimum_height = 34; select.trigger_padding = EdgeInsets::Symmetric(10, 5);
    styles.Set(select);
    auto checkbox = UseEnvironment<CheckboxStyle>(); checkbox.minimum_interactive_size = 28;
    styles.Set(checkbox);
    auto icon = UseEnvironment<IconButtonStyle>(); icon.minimum_interactive_size = 32; icon.state_layer_size = 32;
    styles.Set(icon);
    auto slider = UseEnvironment<SliderStyle>();
    slider.height = 28; slider.track_height = 6; slider.thumb_height = 22;
    slider.hovered_thumb_height = 24; slider.pressed_thumb_height = 24;
    slider.tick_size = 2; styles.Set(slider);
  }
  return Theme(styles, content);
}
static std::vector<MenuEntry> ProjectEntries(const Context& c) {
  return {MenuItem(app::strings::new_project, [=] { ConfirmReplace(c, Example(0)); }),
      MenuItem(app::strings::open, [=] { OpenProject(c); }), MenuItem(app::strings::rename, [=] { ShowRename(c); }),
      MenuItem(app::strings::save_copy, [=] { Export(c, 0); }), MenuItem(app::strings::import_csv, [=] { Import(c); }),
      MenuItem(app::strings::export_svg, [=] { Export(c, 1); }), MenuItem(app::strings::export_time, [=] { Export(c, 2); }),
      MenuItem(app::strings::export_frequency, [=] { Export(c, 3); })};
}
[[huxerui::composable]]
static View ProjectButton() {
  const auto c = UseEnvironment<Context>();
  const auto menu = UseMenu();
  return Button(UseString(app::strings::file_menu) + " ▾")
      .OnClick([=] { menu.Show(ProjectEntries(c)); }).With(menu.Anchor());
}
[[huxerui::composable]]
static View ExamplesSelect(float width = 232, bool single_line = false, std::function<void()> dismiss = {}) {
  const auto c = UseEnvironment<Context>();
  const auto& e = c.experiment.Get(); const auto t = UseTheme();
  const std::vector<StringVariant> names{app::strings::square_example, app::strings::function_example, app::strings::break_example,
    app::strings::lissajous_example, app::strings::polar_example, app::strings::beats_example, app::strings::leakage_example, app::strings::alias_example};
  const bool modified = ExampleModified(e);
  const StringVariant current = e.example_id >= 0 ? names[e.example_id] : StringVariant(app::strings::custom_experiment);
  std::vector<int> choices{0, 1, 2, 3, 4, 5, 6, 7};
  const bool custom = modified || e.example_id < 0;
  if (custom) choices.push_back(-1);
  return Select(choices, custom ? 8 : e.example_id, [=](int id) -> View {
    const auto name = id < 0 ? current : names[id];
    if (single_line) {
      return Row {
        Text(name).Style({Font::System(13), t.colors.on_surface}),
        id < 0 && modified ? View(Text("•").Style({Font::System(13), t.colors.primary})) : View(),
      }.With(Spacing(6), CrossAlign(CrossAxisAlignment::Center), Semantics{.label = name},
          Tooltip(id < 0 && modified ? StringVariant(app::strings::example_modified) : name)).Key(id);
    }
    return Column {
      Text(name).Style({Font::System(13), t.colors.on_surface}),
      Text(id < 0 && modified ? app::strings::example_modified : app::strings::examples)
          .Style({Font::System(10), t.colors.on_surface_variant}),
    }.With(Spacing(2), CrossAlign(CrossAxisAlignment::Stretch), Semantics{.label = name}).Key(id);
  }).Label(app::strings::examples).OnChanged([=](std::size_t index) {
    if (index < 8) {
      if (dismiss) dismiss();
      ConfirmReplace(c, Example(static_cast<int>(index)));
    }
  }).With(Frame{.width = width});
}
[[huxerui::composable]]
static View ToolbarStyles(View content) {
  const auto& t = UseTheme();
  auto tabs = UseEnvironment<SegmentedButtonStyle>();
  tabs.padding = EdgeInsets::Symmetric(12, 6); tabs.minimum_height = 32;
  tabs.label_style = {Font::System(13), t.colors.on_surface_variant};
  tabs.background = Color::Transparent(); tabs.selected_background = t.colors.secondary_container;
  tabs.selected_label = t.colors.on_secondary_container;
  tabs.border = {Color::Transparent(), 0}; tabs.selected_border = {Color::Transparent(), 0};
  tabs.corner_radii = CornerRadii{6};
  auto button = UseEnvironment<ButtonStyle>();
  button.padding = EdgeInsets::Symmetric(10, 6); button.minimum_height = 32;
  button.background = Color::Transparent(); button.corner_radii = CornerRadii{6};
  button.label_style = {Font::System(13), t.colors.on_surface};
  auto indication = button.indication.value_or(t.interactions.indication);
  auto hover_color = t.colors.on_surface; hover_color.alpha = 0.08F;
  auto press_color = t.colors.on_surface; press_color.alpha = 0.12F;
  indication.hover = IndicationLayer{.fill = hover_color, .corner_radii = CornerRadii{6}};
  indication.press = IndicationLayer{.fill = press_color, .corner_radii = CornerRadii{6}};
  indication.focus = IndicationLayer{.fill = press_color, .corner_radii = CornerRadii{6}};
  if (indication.ripple) indication.ripple->color = press_color;
  indication.geometry.clip_corner_radii = CornerRadii{6};
  button.indication = indication;
  auto select = UseEnvironment<SelectStyle>();
  select.trigger_padding = EdgeInsets::Symmetric(10, 6); select.minimum_height = 32;
  select.background = Color::Transparent(); select.border = {Color::Transparent(), 0};
  select.corner_radii = CornerRadii{6}; select.indicator_size = 14;
  ThemeDefinition styles; styles.Set(tabs); styles.Set(button); styles.Set(select);
  return Theme(styles, content);
}
[[huxerui::composable]]
static View MobileToolbar(std::function<void()> settings) {
  const auto c = UseEnvironment<Context>();
  const auto mode = c.experiment.Get().mode;
  const auto& t = UseTheme();
  const auto modes = UseMenu();
  const auto more = UseMenu();
  const auto examples = [=] { c.sheets->Show([=](BottomSheetContext sheet) {
    return ProvideEnvironment(c, Column {
      Row {Label(app::strings::examples, 20, false, true), Spacer(),
        Icon(app::images::close, app::strings::close, [=] { sheet.Dismiss(); })}
          .With(CrossAlign(CrossAxisAlignment::Center)),
      ExamplesSelect(260, false, [=] { sheet.Dismiss(); }),
    }.With(Padding(20), Spacing(16), Frame{.max_width = 400},
        CrossAlign(CrossAxisAlignment::Stretch), SafeAreaPadding()));
  }); };
  auto button = UseEnvironment<ButtonStyle>();
  button.background = Color::Transparent(); button.padding = EdgeInsets::Symmetric(4, 10);
  button.label_style = {Font::System(14).WithWeight(FontWeight::Medium), t.colors.on_surface};
  button.indication = t.interactions.indication;
  ThemeDefinition style; style.Set(button);
  View mode_button = Theme(style, Button(UseString(mode == 0 ? app::strings::functions : app::strings::signals) + " ▾")
      .OnClick([=] {
        modes.Show({
          MenuItem(app::strings::functions, [=] { Change(c, [](auto& e) { e.mode = 0; }, false, false); }).Checked(mode == 0),
          MenuItem(app::strings::signals, [=] { Change(c, [](auto& e) { e.mode = 1; }, false, false); }).Checked(mode == 1),
        });
      }).With(modes.Anchor()));
  return Row {
    mode_button, Spacer(), HistoryActions(),
    Icon(app::images::edit, mode == 0 ? app::strings::expressions : app::strings::signal_controls,
        [=] { ShowEditor(c, c.experiment.Get().mode); }),
    Icon(app::images::more, app::strings::more, [=] {
      more.Show({MenuItem(app::strings::file_menu, ProjectEntries(c)), MenuItem(app::strings::examples, examples),
        MenuItem(app::strings::theme, [=] { Change(c, [](auto& e) { e.dark = !e.dark; }, false, false); }),
        MenuItem(app::strings::settings, settings)});
    }).With(more.Anchor()),
  }.With(Frame{.height = 56}, Padding(EdgeInsets::Symmetric(8, 0)), Spacing(2),
      CrossAlign(CrossAxisAlignment::Center), Background(t.colors.surface_container_low));
}
[[huxerui::composable]]
static View Shell() {
  const auto c = UseEnvironment<Context>();
  const auto& e = c.experiment.Get();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const auto& t = UseTheme();
  const auto window = UseWindow();
  const auto closing = UseState(false);
  const auto waiting_to_close = UseState(false);
  window.OnCloseRequest([=] {
    if (closing.Get() || !c.jobs->saving) return false;
    if (waiting_to_close.Get()) return true;
    waiting_to_close = true;
    c.tasks.Launch([=]() -> Task<void> {
      while (c.jobs->saving) co_await Delay(std::chrono::milliseconds(50));
      waiting_to_close = false;
      if (c.jobs->save_failed) co_return;
      closing = true; window.Close();
    });
    return true;
  });
  const auto settings = [=] { c.sheets->Show([=](BottomSheetContext sheet) {
    return ProvideEnvironment(c, Column {
      Row {Label(app::strings::settings, 20, false, true), Spacer(), Icon(app::images::close, app::strings::close, [=] { sheet.Dismiss(); })}
          .With(CrossAlign(CrossAxisAlignment::Center)),
      Settings(),
    }.With(Padding(20), Spacing(16), Frame{.max_width = 520}, CrossAlign(CrossAxisAlignment::Stretch), SafeAreaPadding()));
  }); };
  View tabs = SegmentedButton({SegmentedButtonItem(app::strings::functions), SegmentedButtonItem(app::strings::signals)}, e.mode)
      .OnChanged([=](std::size_t value) { Change(c, [=](auto& next) { next.mode = static_cast<int>(value); }, false, false); });
  Views header;
  if (Desktop() || !compact) {
    const bool expanded = UseViewportClass() == ViewportClass::Expanded;
    std::vector<View> actions {
      expanded ? View(Row {
        Image(app::images::brand).With(Frame{.width = 22, .height = 22}),
        Label(app::strings::app_name, 14, false, true),
      }.With(Spacing(10), Padding(EdgeInsets{.right = 12}), CrossAlign(CrossAxisAlignment::Center))) : View(),
      ProjectButton(),
      Divider(Axis::Vertical).With(Frame{.height = 18}, Padding(EdgeInsets::Symmetric(4, 0))),
      tabs, ExamplesSelect(expanded ? 210 : 190, true),
      Divider(Axis::Vertical).With(Frame{.height = 18}, Padding(EdgeInsets::Symmetric(4, 0))),
      HistoryActions(),
      Spacer(),
      Icon(app::images::theme, app::strings::theme, [=] { Change(c, [](auto& next) { next.dark = !next.dark; }, false, false); }),
      Icon(app::images::settings, app::strings::settings, settings),
    };
    View bar = Desktop() ? View(WindowTitleBar {std::move(actions)}) : View(Row {std::move(actions)}.With(Frame{.height = 52}));
    header.Add(ToolbarStyles(View(bar).With(Spacing(expanded ? 8 : 4), Padding(EdgeInsets::Symmetric(expanded ? 16 : 8, 0)),
        CrossAlign(CrossAxisAlignment::Center), Background(t.colors.surface_container_low))));
  } else {
    header.Add(MobileToolbar(settings));
  }
  header.Add(Divider());
  if (!c.ready.Get()) header.Add(Column {ProgressCircle(), Label(app::strings::loading)}
      .With(Grow(), Spacing(16), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center)));
  else header.Add(Pager({Workspace(0).Key("functions"), Workspace(1).Key("signals")}, e.mode)
      .DragEnabled(false).OnChanged([=](std::size_t mode) { Change(c, [=](auto& next) { next.mode = static_cast<int>(mode); }, false, false); }).With(Grow()));
  if (!c.error.Get().empty()) header.Add(Row {
    Label(c.error.Get(), 13).With(Grow()), Icon(app::images::close, app::strings::close, [=] { c.error = std::string{}; }),
  }.With(Padding(12), Spacing(12), Background(t.colors.surface_container), CrossAlign(CrossAxisAlignment::Center), Semantics{.live_region = SemanticLiveRegion::Polite}));
  if (!compact) header.Add(Row {Label(c.computing.Get() ? StringVariant(app::strings::updating) : c.status.Get(), 12, true), Spacer(),
    Label(e.name, 12, true).With(Frame{.max_width = 240}),
    Label(e.degrees ? app::strings::degrees : app::strings::radians, 12, true)}.With(Padding(EdgeInsets::Symmetric(24, 8)), Spacing(12)));
  return Column {std::move(header)}.With(CrossAlign(CrossAxisAlignment::Stretch), Background(t.colors.surface), SafeAreaPadding(),
      SystemBarsAppearance{.status_bar_background = t.colors.surface, .navigation_bar_background = t.colors.surface,
        .status_bar_content = e.dark ? SystemBarContentBrightness::Light : SystemBarContentBrightness::Dark,
        .navigation_bar_content = e.dark ? SystemBarContentBrightness::Light : SystemBarContentBrightness::Dark})
      .On<ViewEvents::KeyIntercept>([=](const KeyEvent& event) {
        if (event.type != KeyEventType::Down || !event.modifiers.control || event.modifiers.alt
            || event.modifiers.meta || c.formula_focused.Get()) return false;
        const bool undo = event.key == Key::Z && !event.modifiers.shift;
        const bool redo = (event.key == Key::Z && event.modifiers.shift)
            || (event.key == Key::Y && !event.modifiers.shift);
        if (!undo && !redo) return false;
        EndEdit(c);
        if ((redo ? c.history->redo : c.history->undo).empty()) return false;
        Undo(c, redo);
        return true;
      }).On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
        if (!event.modifiers.control) return false;
        if (event.key == Key::S) { Export(c, 0); return true; }
        if (event.key == Key::O) { OpenProject(c); return true; }
        return false;
      });
}
[[huxerui::composable]]
static View AppContent(State<Experiment> experiment) {
  const auto dialog_handle = UseDialog();
  const auto sheet_handle = UseBottomSheet();
  const auto dialogs = UseState(std::make_shared<DialogHandle>(dialog_handle)).Get();
  const auto sheets = UseState(std::make_shared<BottomSheetHandle>(sheet_handle)).Get();
  // Existing callbacks share handles refreshed with the current themed environment.
  *dialogs = dialog_handle; *sheets = sheet_handle;
  const Context c{
    .experiment = experiment, .plots = UseState(std::shared_ptr<const PlotResult>{}),
    .signal = UseState(std::shared_ptr<const SignalResult>{}), .revision = UseState(0), .plot_generation = UseState(0), .signal_generation = UseState(0),
    .ready = UseState(false), .computing = UseState(false), .trace = UseState(false),
    .formula_focused = UseState(false), .error = UseState(std::string{}),
    .status = UseState(StringVariant(app::strings::ready)), .selected_plot = UseState(0), .selected_curve = UseState(0), .selected_bin = UseState(8),
    .history = UseState(std::make_shared<History>()).Get(), .jobs = UseState(std::make_shared<Jobs>()).Get(),
    .tasks = UseTaskScope(), .directories = UseApplication().Directories(), .picker = UseService<FilePicker>(),
    .dialogs = dialogs, .sheets = sheets};
  Lifecycle([=] {
    c.tasks.Launch([c]() -> Task<void> {
      const auto saved = co_await c.directories.data_directory.Child("experiment.hgraph").ReadStringAsync();
      if (saved.Succeeded()) {
        try { c.experiment = Deserialize(saved.Value()); c.status = app::strings::saved; }
        catch (const std::exception& error) { c.error = error.what(); }
        if (!c.error.Get().empty()) {
          const auto previous = co_await c.directories.data_directory.Child("previous.hgraph").ReadStringAsync();
          if (previous.Succeeded()) { try { c.experiment = Deserialize(previous.Value()); } catch (...) {} }
        }
      }
      c.ready = true; Refresh(c);
    });
  });
  return ProvideEnvironment(c, Shell());
}
View App() {
  const auto experiment = UseState(Example(0));
  return MaterialTheme(GraphTheme(experiment.Get().dark), GraphControlStyles(AppContent(experiment)));
}
} // namespace graph::ui

static huxerui::AppOptions Options() {
  huxerui::AppOptions options;
  options.window.title = "Huxer Graph";
  options.window.initial_size = {1420, 980};
  options.window.minimum_size = huxerui::Size{760, 600};
  options.window.chrome_mode = graph::ui::Desktop() ? huxerui::WindowChromeMode::Custom : huxerui::WindowChromeMode::System;
  options.window.title_bar_height = 52;
  options.window.caption_labels = {.minimize = app::strings::minimize, .toggle_maximize = app::strings::maximize, .close = app::strings::close};
  options.viewport_breakpoints = {.medium_width = 720, .expanded_width = 1120};
  options.show_debug_overlay = false;
  return options;
}
const huxerui::Application application{graph::ui::App, Options()};
