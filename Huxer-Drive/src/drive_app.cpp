#include "drive_ui.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace huxer_drive {

bool DesktopWindow() {
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
  return false;
#else
  return true;
#endif
}

[[huxerui::composable]]
static View NavigationRow(Area area, bool expanded) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  const auto hover = UseState(false);
  const bool active = context.area.Get() == area;
  const auto color = active ? theme.colors.primary : theme.colors.on_surface;
  const auto action = [=] { GoArea(context, area); };
  Views content;
  content.Add(Glyph(AreaIcon(area), 22, color));
  if (expanded) {
    content.Add(Text(AreaLabel(area)).Style({Font::System(14).WithWeight(active ? FontWeight::Medium : FontWeight::Regular), color}));
    content.Add(Spacer());
    if (area == Area::Transfers) {
      std::size_t pending = 0;
      for (const auto& transfer : context.transfers.Get()) if (!transfer.completed) ++pending;
      if (pending) content.Add(Text(std::to_string(pending)).Style({Font::System(11), Color::White()})
          .Align(TextAlign::Center).VerticalAlign(TextVerticalAlign::Center)
          .With(Frame{.width = 21, .height = 21}, Background(theme.colors.primary), CornerRadius(11)));
    }
  }
  return Row{std::move(content)}.With(Spacing(16), Padding(EdgeInsets::Symmetric(12, 0)), Frame{.height = 44},
      CrossAlign(CrossAxisAlignment::Center), Background(active ? theme.colors.secondary_container : hover.Get() ? theme.colors.surface_container_high : Color::Transparent()),
      CornerRadius(8), Focusable{}, Semantics{.role = SemanticRole::Button, .label = AreaLabel(area), .selected = active})
      .OnClick(action).On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
        if (event.key == Key::Enter || event.key == Key::Space) { action(); return true; } return false;
      }).On<ViewEvents::Hover>([=](const HoverEvent& event) { hover = event.type != HoverEventType::Leave; });
}

[[huxerui::composable]]
static View Sidebar(bool expanded) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  Views rows;
  rows.Add(Row{Avatar(), expanded ? View(Column{Label("Alex Morgan", 14, false, true), Label(app::strings::personal, 12, true)}.With(Spacing(5))) : View()}
      .With(Padding(EdgeInsets::Symmetric(expanded ? 20 : 16, 24)), Spacing(14), CrossAlign(CrossAxisAlignment::Center)));
  Views navigation;
  for (const auto area : {Area::Files, Area::Recent, Area::Favorites, Area::Shares, Area::Transfers}) navigation.Add(NavigationRow(area, expanded).Key(static_cast<int>(area)));
  navigation.Add(Divider().With(Padding(EdgeInsets::Symmetric(6, 12))));
  navigation.Add(NavigationRow(Area::Trash, expanded));
  rows.Add(Column{std::move(navigation)}.With(Spacing(3), Padding(EdgeInsets::Symmetric(12, 0))));
  rows.Add(Spacer());
  const auto bytes = StoredBytes(context.data.Get());
  if (expanded) {
    rows.Add(Column{Label(app::strings::storage, 12), Row{ProgressBar(static_cast<float>(bytes) / kCapacity).With(Grow())},
        Label(StringVariant::Format(app::strings::storage_usage, FormatSize(bytes)), 12, true),
        ActionButton({}, app::strings::demo, [=] { GoArea(context, Area::Storage); }, ActionStyle::Selected)}
        .With(Padding(20), Spacing(10), Focusable{}, Semantics{.role = SemanticRole::Button, .label = app::strings::storage})
        .OnClick([=] { GoArea(context, Area::Storage); }));
  } else rows.Add(IconButton(app::images::disk, app::strings::storage).OnClick([=] { GoArea(context, Area::Storage); }).With(Padding(18)));
  return Column{std::move(rows)}.With(Frame{.width = expanded ? 232.0F : 76.0F}, Background(theme.colors.surface_container_low));
}

[[huxerui::composable]]
static View RootWorkspace() {
  const auto context = UseEnvironment<DriveContext>();
  const Area area = context.area.Get();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  if (compact && (area == Area::Trash || area == Area::Storage || area == Area::Settings)) return Workspace(0, area);
  std::vector<View> pages;
  const std::vector<Area> areas = compact ? std::vector<Area>{Area::Files, Area::Shares, Area::Transfers}
      : std::vector<Area>{Area::Files, Area::Recent, Area::Favorites, Area::Shares, Area::Transfers, Area::Trash, Area::Storage, Area::Settings};
  std::size_t selected = 0;
  for (std::size_t index = 0; index < areas.size(); ++index) {
    pages.push_back(Workspace(0, areas[index]).Key(static_cast<int>(areas[index])));
    if (areas[index] == area) selected = index;
  }
  return Pager(std::move(pages), selected).ScrollAxis(compact ? Axis::Horizontal : Axis::Vertical)
      .DragEnabled(compact && context.selected.Get().empty() && context.query.Get().empty())
      .OnChanged([=](std::size_t index) {
        if (index < areas.size()) GoArea(context, compact && index == 0 ? context.file_area.Get() : areas[index]);
      }).With(Grow()).Key(compact ? "mobile-pages" : "desktop-pages");
}

[[huxerui::composable]]
View DriveRoot() {
  const auto context = UseEnvironment<DriveContext>();
  const auto viewport = UseViewportClass();
  const auto& theme = UseTheme();
  View page = NavigationStack<Route>([] { return RootWorkspace(); }, context.path,
      [](const Route& route) { return route.preview ? Preview(route.id) : Workspace(route.id); });
  Views content;
  if (!context.ready.Get()) {
    content.Add(Column{context.error.Get() == Error::None ? View(ProgressCircle()) : View(Glyph(app::images::folder, 52)),
        Label(context.error.Get() == Error::None ? app::strings::loading : ErrorLabel(context.error.Get()), 17),
        context.error.Get() == Error::None || context.busy.Get() ? View() : View(Button(app::strings::retry).OnClick([=] {
          Run(context, [=] { return context.store->Load(); }, [=] { context.ready = true; });
        }))}.With(Grow(), Spacing(20), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center)));
  } else {
    content.Add(viewport == ViewportClass::Compact ? std::move(page).With(Grow())
        : View(Row{Sidebar(viewport == ViewportClass::Expanded), std::move(page).With(Grow())}.With(Grow())));
    const bool root_navigation = context.path.Get().Routes().empty() && context.selected.Get().empty()
        && context.area.Get() != Area::Trash && context.area.Get() != Area::Storage && context.area.Get() != Area::Settings;
    if (viewport == ViewportClass::Compact && root_navigation) content.Add(Column{Divider(), NavigationBar({
        NavigationItem(app::images::folder, app::strings::nav_files), NavigationItem(app::images::share, app::strings::nav_shares), NavigationItem(app::images::transfer, app::strings::transfers)},
        context.area.Get() == Area::Shares ? 1 : context.area.Get() == Area::Transfers ? 2 : 0)
        .OnChanged([=](std::size_t index) { GoArea(context, index == 1 ? Area::Shares : index == 2 ? Area::Transfers : context.file_area.Get()); })});
  }
  const bool failure = context.error.Get() != Error::None;
  if (viewport != ViewportClass::Compact || failure) content.Add(Row{Glyph(failure ? app::images::more : app::images::check, 15, failure ? theme.colors.error : theme.colors.on_surface_variant),
      Label(failure ? ErrorLabel(context.error.Get()) : context.busy.Get() ? app::strings::busy : app::strings::success, 12, true).With(Grow()),
      failure ? View(IconButton(app::images::close, app::strings::close).OnClick([=] { context.error = Error::None; })) : View(),
      viewport != ViewportClass::Compact ? View(IconButton(app::images::transfer, app::strings::transfers).OnClick([=] { context.transfer_panel = !context.transfer_panel.Get(); })) : View()}
      .With(Padding(EdgeInsets::Symmetric(28, 4)), Frame{.min_height = 32}, Spacing(8), CrossAlign(CrossAxisAlignment::Center), Semantics{.live_region = SemanticLiveRegion::Polite}));
  View shell = Column{std::move(content)}.With(Grow(), CrossAlign(CrossAxisAlignment::Stretch), Background(theme.colors.surface));
  if (context.transfer_panel.Get() && viewport != ViewportClass::Compact && !context.transfers.Get().empty() && context.area.Get() != Area::Transfers) {
    shell = Stack{std::move(shell), Column{Spacer(), Row{Spacer(), Column{
        Row{Label(app::strings::transfers, 15, false, true), Spacer(), IconButton(app::images::chevron_down, app::strings::close).OnClick([=] { context.transfer_panel = false; })}.With(CrossAlign(CrossAxisAlignment::Center)),
        TransferList(true), QuietButton(app::strings::transfers, [=] { GoArea(context, Area::Transfers); context.transfer_panel = false; })}
        .With(Padding(16), Spacing(12), Frame{.width = 350}, Background(theme.colors.surface), Border{theme.colors.outline, 1}, CornerRadius(10),
              Shadow{.color = Color::Rgb(36, 65, 101, 0.12F), .blur_radius = 20})}.With(Padding(EdgeInsets{.top = 0, .right = 20, .bottom = 40, .left = 0}))}};
  }
  if (DesktopWindow() || viewport != ViewportClass::Compact) {
    Views bar;
    bar.Add(Row{Image(app::images::brand).With(Frame{.width = 22, .height = 25}), Label(app::strings::app_name, 14, false, true)}
        .With(Spacing(10), Frame{.width = viewport == ViewportClass::Expanded ? 212.0F : 180.0F}, CrossAlign(CrossAxisAlignment::Center)));
    if (viewport != ViewportClass::Compact) bar.Add(Row{Spacer(), DriveSearch().With(Frame{.width = viewport == ViewportClass::Expanded ? 400.0F : 280.0F}), Spacer()}.With(Grow()));
    else bar.Add(Spacer());
    bar.Add(Row{
        IconButton(app::images::sun, app::strings::theme).With(Frame{.width = 36, .height = 36})
            .OnClick([=] { context.dark = !context.dark.Get(); }),
        IconButton(app::images::settings, app::strings::settings).With(Frame{.width = 36, .height = 36})
            .OnClick([=] { GoArea(context, Area::Settings); })}
        .With(Spacing(6), CrossAlign(CrossAxisAlignment::Center), Padding(EdgeInsets::Symmetric(14, 0))));
    View titlebar = DesktopWindow() ? View(WindowTitleBar{std::move(bar)}.With(CrossAlign(CrossAxisAlignment::Center)))
        : View(Row{std::move(bar)}.With(Frame{.height = 52}, CrossAlign(CrossAxisAlignment::Center)));
    shell = Column{std::move(titlebar).With(Padding(EdgeInsets::Symmetric(20, 0)), Background(theme.colors.surface_container_low)), Divider(), std::move(shell).With(Grow())};
  }
  return std::move(shell).With(SafeAreaPadding(), SystemBarsAppearance{
      .status_bar_background = theme.colors.surface, .navigation_bar_background = theme.colors.surface,
      .status_bar_content = context.dark.Get() ? SystemBarContentBrightness::Light : SystemBarContentBrightness::Dark,
      .navigation_bar_content = context.dark.Get() ? SystemBarContentBrightness::Light : SystemBarContentBrightness::Dark});
}

[[huxerui::composable]]
View App() {
  const auto files = UseService<FileSystem>();
  const auto sample = UseRawResource(app::raw::coastal_study_png);
  const auto document = UseRawResource(app::raw::brand_guidelines_pdf);
  const auto archive = UseRawResource(app::raw::launch_assets_zip);
  const auto store = UseState(std::make_shared<DriveStore>(files->Directories().data_directory.Child("drive"), sample, document, archive)).Get();
  const DriveContext context{
      .data = UseState(Snapshot{}), .ready = UseState(false), .busy = UseState(false), .error = UseState(Error::None),
      .area = UseState(Area::Files), .file_area = UseState(Area::Files), .path = UseState(NavigationPath<Route>{}), .search = UseState(TextEditingValue::FromText("")),
      .query = UseState(std::string{}), .searching = UseState(false), .filter = UseState(0), .sort = UseState(1), .reverse = UseState(false),
      .grid = UseState(false), .dark = UseState(false), .selected = UseState(std::vector<Id>{}),
      .transfers = UseState(std::vector<Transfer>{}), .transfer_panel = UseState(false), .conflict = UseState(Conflict::KeepBoth),
      .tasks = UseTaskScope(), .store = store, .picker = UseService<FilePicker>(), .dialogs = UseDialog()};
  const auto application = UseApplication();
  const auto startup = application.StartupActivation();
  Lifecycle([=] { Run(context, [=] { return store->Load(); }, [=] {
    context.ready = true;
    if (const auto* payload = std::get_if<FileActivation>(&startup)) ImportFiles(context, payload->files, 0);
  }); });
  application.OnActivation([=](ApplicationActivation activation) {
    if (const auto* payload = std::get_if<FileActivation>(&activation)) if (context.ready.Get()) ImportFiles(context, payload->files, 0);
  });
  Environment environment; environment.Set(context);
  return Theme(DriveTheme(context.dark.Get(), UseViewportClass() == ViewportClass::Compact), ProvideEnvironment(std::move(environment), DriveRoot()));
}

} // namespace huxer_drive
