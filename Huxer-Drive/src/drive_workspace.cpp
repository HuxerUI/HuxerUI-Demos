#include "drive_ui.h"

#include <algorithm>
#include <array>
#include <set>

namespace huxer_drive {

[[huxerui::composable]]
static View FileMenu(Entry item, bool compact) {
  const auto menu = UseMenu();
  const auto sheets = UseBottomSheet();
  const auto context = UseEnvironment<DriveContext>();
  const auto open = [=] {
    std::vector<SheetAction> actions;
    if (context.area.Get() == Area::Trash) {
      actions.push_back({app::images::restore, app::strings::restore, [=] { Change(context, [=](Snapshot& next) { return Trash(next, {item.id}, true); }); }});
      actions.push_back({app::images::trash, app::strings::purge, [=] { ConfirmPurge(context, {item.id}); }, true});
    } else {
      actions.push_back({app::images::info, app::strings::details, [=] { Navigate(context, item.id, true); }});
      if (!item.folder) actions.push_back({app::images::download, app::strings::download, [=] { Download(context, item.id); }});
      actions.push_back({app::images::star, app::strings::favorites, [=] { Change(context, [=](Snapshot& next) { auto* value = Find(next, item.id); if (value) value->favorite = !value->favorite; return Error::None; }); }});
      actions.push_back({app::images::share, app::strings::share, [=] { ShareDialog(context, item.id); }});
      actions.push_back({app::images::edit, app::strings::rename, [=] { RenameDialog(context, item.id, 0, false); }});
      actions.push_back({app::images::folder, app::strings::move, [=] { DestinationDialog(context, {item.id}, false); }});
      actions.push_back({app::images::copy, app::strings::copy, [=] { DestinationDialog(context, {item.id}, true); }});
      actions.push_back({app::images::trash, app::strings::trash_action, [=] { Change(context, [=](Snapshot& next) { return Trash(next, {item.id}, false); }); }, true});
    }
    if (compact) { ShowActions(sheets, item.name, std::move(actions)); return; }
    std::vector<MenuEntry> entries;
    for (const auto& action : actions) entries.emplace_back(MenuItem(action.label, action.action));
    menu.Show(std::move(entries));
  };
  return IconButton(app::images::more, app::strings::more).With(menu.Anchor(),
      Frame{.width = compact ? 44.0F : 36.0F, .height = compact ? 44.0F : 36.0F}, Enabled(!context.busy.Get())).OnClick(open);
}

static void ToggleSelection(const DriveContext& context, Id id) {
  context.selected.Update([=](auto& ids) {
    auto found = std::ranges::find(ids, id);
    if (found == ids.end()) ids.push_back(id); else ids.erase(found);
  });
}
static std::vector<Entry> Project(const DriveContext& context, Id folder, Area area) {
  const Snapshot& data = context.data.Get();
  const std::string query = Fold(context.query.Get());
  std::vector<Entry> entries;
  for (const auto& item : data.entries) {
    const bool trashed = InTrash(data, item.id);
    if (area == Area::Trash) {
      if (!item.trashed || InTrash(data, item.parent)) continue;
    } else {
      if (trashed) continue;
      if (area == Area::Files && query.empty() && item.parent != folder) continue;
      if (area == Area::Favorites && !item.favorite) continue;
      if (area == Area::Shares && !item.share) continue;
    }
    if (!query.empty() && Fold(item.name).find(query) == std::string::npos) continue;
    const Kind kind = FileKind(item);
    const int filter = context.filter.Get();
    if (filter == 1 && kind != Kind::Text && kind != Kind::Document) continue;
    if (filter == 2 && kind != Kind::Image) continue;
    if (filter == 3 && kind != Kind::Other) continue;
    entries.push_back(item);
  }
  std::ranges::sort(entries, [&](const Entry& a, const Entry& b) {
    if (a.id == b.id) return false;
    if (area == Area::Files && a.folder != b.folder) return a.folder;
    bool less;
    if (area == Area::Recent) {
      const auto ta = std::max(a.modified, a.accessed), tb = std::max(b.modified, b.accessed);
      less = ta == tb ? a.id < b.id : ta > tb;
    } else if (context.sort.Get() == 1 && a.modified != b.modified) less = a.modified > b.modified;
    else if (context.sort.Get() == 2 && !a.folder && !b.folder && a.versions.back().size != b.versions.back().size) less = a.versions.back().size > b.versions.back().size;
    else less = Fold(a.name) == Fold(b.name) ? a.id < b.id : Fold(a.name) < Fold(b.name);
    return context.reverse.Get() ? !less : less;
  });
  return entries;
}
[[huxerui::composable]]
static View FileTile(Entry item, bool grid, bool compact, Area area) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  const auto hover = UseState(false);
  const bool trash = area == Area::Trash;
  const bool selected = std::ranges::find(context.selected.Get(), item.id) != context.selected.Get().end();
  const auto open = [=] {
    if (trash) return;
    if (!context.selected.Get().empty()) { ToggleSelection(context, item.id); return; }
    if (item.folder) Navigate(context, item.id, false);
    else {
      Navigate(context, item.id, true);
      if (!context.busy.Get()) Change(context, [=](Snapshot& next) { auto* value = Find(next, item.id); if (value) value->accessed = Now(); return Error::None; });
    }
  };
  const bool selecting = !context.selected.Get().empty();
  View more = compact && selecting ? View() : FileMenu(item, compact);
  View checkbox = Checkbox(selected).With(Semantics{.label = item.name}, Frame{.width = compact ? 32.0F : 18.0F, .height = compact ? 44.0F : 18.0F})
      .OnChanged([=](bool) { ToggleSelection(context, item.id); });
  const StringVariant metadata = item.folder ? StringVariant::Format(app::strings::items, std::ranges::count_if(context.data.Get().entries,
      [&](const Entry& entry) { return entry.parent == item.id && !entry.trashed; })) : StringVariant(FormatSize(item.versions.back().size));
  Views body;
  if (grid) {
    body.Add(Row{!compact || selecting ? std::move(checkbox) : View(), Spacer(), std::move(more)}.With(CrossAlign(CrossAxisAlignment::Center)));
    body.Add(Row{FileArtwork(item, compact ? 74 : 94)}.With(Frame{.height = compact ? 100.0F : 128.0F}, MainAlign(MainAxisAlignment::Center))
        .With(Focusable{}, Semantics{.role = SemanticRole::Button, .label = item.name}).OnClick(open)
        .On<ViewEvents::KeyDown>([=](const KeyEvent& event) { if (event.key == Key::Enter || event.key == Key::Space) { open(); return true; } return false; }));
    body.Add(Column{SingleLine(item.name, 14, false, true), SingleLine(metadata, 12, true)}.With(Spacing(5), Padding(EdgeInsets::Symmetric(6, 8)), CrossAlign(CrossAxisAlignment::Stretch)));
  } else {
    if (!compact || !context.selected.Get().empty()) body.Add(std::move(checkbox));
    body.Add(FileArtwork(item, 34).OnClick(open));
    Views title;
    title.Add(Row{SingleLine(item.name, compact ? 15 : 14).With(Grow()),
        compact && item.favorite ? Glyph(app::images::star, 14, Color::Rgb(235, 170, 30)) : View()}.With(Spacing(5), CrossAlign(CrossAxisAlignment::Center)));
    if (compact) title.Add(SingleLine(UseString(metadata) + " · " + UseString(ShortDate(item.modified)), 12, true));
    else if (item.folder) title.Add(Label(metadata, 12, true));
    body.Add(Column{std::move(title)}.With(Grow(), Spacing(4), Padding(EdgeInsets::Symmetric(0, 7)), ClipChildren(), CrossAlign(CrossAxisAlignment::Stretch))
        .With(Focusable{}, Semantics{.role = SemanticRole::Button, .label = item.name}).OnClick(open)
        .On<ViewEvents::KeyDown>([=](const KeyEvent& event) { if (event.key == Key::Enter || event.key == Key::Space) { open(); return true; } return false; }));
    if (!compact) {
      body.Add(Label(ShortDate(item.modified), 13, true).With(Frame{.width = 170}));
      body.Add(Label(item.folder ? StringVariant("—") : metadata, 13, true).With(Frame{.width = 82}));
      body.Add(Row{item.favorite ? Glyph(app::images::star, 20) : item.share ? Glyph(app::images::share, 18) : View()}
          .With(Frame{.width = 28}, MainAlign(MainAxisAlignment::Center)));
    }
    body.Add(std::move(more));
  }
  View tile = grid ? View(Column{std::move(body)}.With(Padding(10), CrossAlign(CrossAxisAlignment::Stretch),
      Border{theme.colors.outline, 1}, CornerRadius(10)))
                   : View(Row{std::move(body)}.With(Spacing(compact ? 12 : 16), CrossAlign(CrossAxisAlignment::Center),
                       Padding(EdgeInsets::Symmetric(compact ? 0 : 4, 0)), Frame{.min_height = compact ? 64.0F : 56.0F}));
  tile = std::move(tile).With(Background(selected ? (compact && !context.dark.Get() ? Color::Rgb(237, 244, 255) : theme.colors.secondary_container) : hover.Get() ? theme.colors.surface_container : theme.colors.surface),
       CornerRadius(compact && !grid ? 0 : 7), Semantics{.label = item.name, .selected = selected}, LongPressGesture{})
       .On<LongPressEvents::Started>([=](const LongPressEvent&) { ToggleSelection(context, item.id); })
       .On<ViewEvents::Hover>([=](const HoverEvent& event) { hover = event.type != HoverEventType::Leave; });
  if (!trash && !compact) {
    tile = std::move(tile).With(DragSource(item.id));
    if (item.folder) tile = std::move(tile).With(DropTarget::Accepts<Id>([=](Id id) { return id != item.id && !Descendant(context.data.Get(), item.id, id); }))
        .On<DropEvents<Id>::Dropped>([=](const Id& id, const DropEvent&) { Change(context, [=](Snapshot& next) { return Relocate(next, {id}, item.id, false); }); });
  }
  return std::move(tile).Key(std::to_string(item.id));
}

[[huxerui::composable]]
static View SortButton(bool compact) {
  const auto context = UseEnvironment<DriveContext>();
  const auto menu = UseMenu();
  const auto sheets = UseBottomSheet();
  const auto open = [=] {
    if (compact) {
      ShowActions(sheets, app::strings::sort, {
        {context.sort.Get() == 0 ? app::images::check : app::images::sort_both, app::strings::name, [=] { context.sort = 0; }},
        {context.sort.Get() == 1 ? app::images::check : app::images::sort_both, app::strings::last_modified, [=] { context.sort = 1; }},
        {context.sort.Get() == 2 ? app::images::check : app::images::sort_both, app::strings::size, [=] { context.sort = 2; }},
        {context.reverse.Get() ? app::images::check : app::images::sort_both, app::strings::descending, [=] { context.reverse = !context.reverse.Get(); }}
      });
      return;
    }
    menu.Show({MenuItem(app::strings::name, [=] { context.sort = 0; }).Checked(context.sort.Get() == 0),
               MenuItem(app::strings::last_modified, [=] { context.sort = 1; }).Checked(context.sort.Get() == 1),
               MenuItem(app::strings::size, [=] { context.sort = 2; }).Checked(context.sort.Get() == 2),
               MenuItem(app::strings::descending, [=] { context.reverse = !context.reverse.Get(); }).Checked(context.reverse.Get())});
  };
  if (compact) return IconButton(app::images::sort_both, app::strings::sort).With(menu.Anchor(), Frame{.width = 44, .height = 44}).OnClick(open);
  return ActionButton(app::images::sort_both,
      context.sort.Get() == 0 ? app::strings::name : context.sort.Get() == 1 ? app::strings::last_modified : app::strings::size,
      open, ActionStyle::Outline, true).With(menu.Anchor());
}

[[huxerui::composable]]
static View ViewSwitch(bool compact) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  if (compact) return IconButton(context.grid.Get() ? app::images::list : app::images::grid, context.grid.Get() ? app::strings::list : app::strings::grid)
      .With(Frame{.width = 44, .height = 44}).OnClick([=] { context.grid = !context.grid.Get(); });
  return Row{
      IconButton(app::images::list, app::strings::list).With(Background(context.grid.Get() ? theme.colors.surface : theme.colors.secondary_container))
          .OnClick([=] { context.grid = false; }),
      IconButton(app::images::grid, app::strings::grid).With(Background(context.grid.Get() ? theme.colors.secondary_container : theme.colors.surface))
          .OnClick([=] { context.grid = true; })
  }.With(Frame{.width = 72, .height = 36}, Border{theme.colors.outline, 1}, CornerRadius(8), ClipChildren());
}

[[huxerui::composable]]
static View UploadButton(Id folder, bool floating) {
  const auto context = UseEnvironment<DriveContext>();
  const auto menu = UseMenu();
  const auto sheets = UseBottomSheet();
  const auto open = [=] {
    if (floating) {
      std::vector<SheetAction> actions{
        {app::images::upload, app::strings::upload_files, [=] { Upload(context, folder); }},
        {app::images::folder, app::strings::new_folder, [=] { RenameDialog(context, 0, folder, true); }}};
      if (context.picker->CanOpenDirectories()) actions.push_back({app::images::folder, app::strings::upload_folder, [=] { Upload(context, folder, 0, true); }});
      ShowActions(sheets, app::strings::add_files, std::move(actions));
      return;
    }
    std::vector<MenuEntry> items{
        MenuItem(app::strings::upload_files, [=] { Upload(context, folder); }),
        MenuItem(app::strings::new_folder, [=] { RenameDialog(context, 0, folder, true); })};
    if (context.picker->CanOpenDirectories()) items.emplace_back(MenuItem(app::strings::upload_folder, [=] { Upload(context, folder, 0, true); }));
    menu.Show(std::move(items));
  };
  if (!floating) return ActionButton(app::images::plus, app::strings::upload, open, ActionStyle::Primary)
      .With(menu.Anchor(), Frame{.min_width = 124}, Enabled(!context.busy.Get()));
  return Row{Glyph(app::images::plus, 27, Color::White())}
      .With(Frame{.width = 56, .height = 56}, MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center),
            Background(UseTheme().colors.primary), CornerRadius(28),
            Shadow{.color = Color::Rgb(0, 71, 200, 0.2F), .blur_radius = 12}, menu.Anchor(),
            Enabled(!context.busy.Get()), Focusable{}, Semantics{.role = SemanticRole::Button, .label = app::strings::upload})
      .OnClick(open).On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
        if (event.key == Key::Enter || event.key == Key::Space) { open(); return true; } return false;
      });
}

[[huxerui::composable]]
static View FolderTitle(Entry folder) {
  const auto context = UseEnvironment<DriveContext>();
  const auto sheets = UseBottomSheet();
  const auto open = [=] {
    std::vector<SheetAction> ancestors{{app::images::folder, app::strings::files, [=] { GoArea(context, Area::Files); }}};
    std::vector<Entry> chain;
    for (auto* entry = Find(context.data.Get(), folder.parent); entry; entry = Find(context.data.Get(), entry->parent)) chain.push_back(*entry);
    std::ranges::reverse(chain);
    for (const auto& entry : chain) ancestors.push_back({app::images::folder, entry.name, [=] {
      GoArea(context, Area::Files); Navigate(context, entry.id, false);
    }});
    ShowActions(sheets, app::strings::location, std::move(ancestors));
  };
  return Row{SingleLine(folder.name, 20, false, true).With(Grow()), Glyph(app::images::chevron_down, 15)}
      .With(Grow(), Frame{.height = 48}, Spacing(4), CrossAlign(CrossAxisAlignment::Center), Focusable{},
            Semantics{.role = SemanticRole::Button, .label = folder.name})
      .OnClick(open).On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
        if (event.key == Key::Enter || event.key == Key::Space) { open(); return true; } return false;
      });
}

[[huxerui::composable]]
static View SelectionBar() {
  const auto context = UseEnvironment<DriveContext>();
  const auto sheets = UseBottomSheet();
  const auto ids = context.selected.Get();
  Views actions;
  if (context.area.Get() == Area::Trash) {
    actions.Add(MobileAction(app::images::restore, app::strings::restore, [=] {
      Change(context, [=](Snapshot& next) { return Trash(next, ids, true); }, [=] { context.selected = std::vector<Id>{}; });
    }));
    actions.Add(MobileAction(app::images::trash, app::strings::purge, [=] { ConfirmPurge(context, ids); }, true));
  } else {
    actions.Add(MobileAction(app::images::share, app::strings::share, [=] {
      if (ids.size() == 1) { ShareDialog(context, ids.front()); return; }
      std::vector<SheetAction> choices;
      for (Id id : ids) if (const auto* item = Find(context.data.Get(), id))
        choices.push_back({app::images::share, item->name, [=] { ShareDialog(context, id); }});
      ShowActions(sheets, app::strings::choose_share, std::move(choices));
    }));
    actions.Add(MobileAction(app::images::download, app::strings::download, [=] {
      DownloadSelection(context, ids);
    }).With(Enabled(std::ranges::any_of(ids, [&](Id id) { const auto* item = Find(context.data.Get(), id); return item && !item->folder; }))));
    actions.Add(MobileAction(app::images::folder, app::strings::move, [=] { DestinationDialog(context, ids, false); }));
    actions.Add(MobileAction(app::images::trash, app::strings::delete_short, [=] {
      Change(context, [=](Snapshot& next) { return Trash(next, ids, false); }, [=] { context.selected = std::vector<Id>{}; });
    }, true));
    actions.Add(MobileAction(app::images::more, app::strings::more_short, [=] {
      ShowActions(sheets, StringVariant::Format(app::strings::selected, ids.size()), {
        {app::images::copy, app::strings::copy, [=] { DestinationDialog(context, ids, true); }},
        {app::images::star, app::strings::favorites, [=] { Change(context, [=](Snapshot& next) {
          for (Id id : ids) if (auto* item = Find(next, id)) item->favorite = true;
          return Error::None;
        }); }} });
    }));
  }
  return Column{Divider(), Row{std::move(actions)}.With(Frame{.height = 64}, Padding(EdgeInsets::Symmetric(8, 0)), Enabled(!context.busy.Get()))}
      .With(Frame{.height = 65});
}

[[huxerui::composable]]
static View StorageSummary() {
  const auto context = UseEnvironment<DriveContext>();
  const auto used = StoredBytes(context.data.Get());
  Views breakdown;
  std::set<Id> counted;
  for (const auto kind : {Kind::Image, Kind::Document, Kind::Other}) {
    std::uint64_t bytes = 0;
    for (const auto& entry : context.data.Get().entries)
      if (FileKind(entry) == kind || (kind == Kind::Document && FileKind(entry) == Kind::Text))
        for (const auto& version : entry.versions) if (counted.insert(version.blob).second) bytes += version.size;
    breakdown.Add(Column{Row{Glyph(kind == Kind::Image ? app::images::image : app::images::file, 23),
        Label(kind == Kind::Image ? app::strings::images : kind == Kind::Other ? app::strings::other : app::strings::documents).With(Grow()),
        Label(FormatSize(bytes), 14, true)}.With(Frame{.height = 64}, Spacing(14), CrossAlign(CrossAxisAlignment::Center)), Divider()});
  }
  return ScrollView{Column{Label(StringVariant::Format(app::strings::storage_usage, FormatSize(used)), 24, false, true),
      Row{ProgressBar(static_cast<float>(used) / kCapacity).With(Grow())}, Label(app::strings::storage_hint, 14, true),
      Column{std::move(breakdown)}}.With(Spacing(20), Padding(20), CrossAlign(CrossAxisAlignment::Stretch))}.With(Grow(), ScrollBar());
}

[[huxerui::composable]]
static View EmptyFiles(Area area) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  ImageResource icon = app::images::folder;
  StringResource title = app::strings::empty;
  StringResource hint = app::strings::empty_hint;
  if (!context.query.Get().empty() || context.filter.Get() != 0) {
    icon = app::images::search; title = app::strings::no_matches; hint = app::strings::no_matches_hint;
  } else if (area == Area::Trash) {
    icon = app::images::trash; title = app::strings::trash_empty; hint = app::strings::trash_empty_hint;
  } else if (area == Area::Favorites) {
    icon = app::images::star; title = app::strings::favorites_empty; hint = app::strings::favorites_empty_hint;
  } else if (area == Area::Shares) {
    icon = app::images::share; title = app::strings::shares_empty; hint = app::strings::shares_empty_hint;
  } else if (area == Area::Recent) {
    icon = AreaIcon(Area::Recent); title = app::strings::recent_empty; hint = app::strings::recent_empty_hint;
  }
  return Column{
      Glyph(icon, 44, theme.colors.primary).With(Padding(18), Background(theme.colors.surface_container_low), CornerRadius(20)),
      Text(title).Style({Font::System(20).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}).Align(TextAlign::Center),
      Text(hint).Style({Font::System(14), theme.colors.on_surface_variant}).Align(TextAlign::Center).With(Frame{.max_width = 360})
  }.With(Grow(), Padding(24), Spacing(14), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center));
}

[[huxerui::composable]]
static View MobileFiles(Id folder, Area area, bool floating) {
  const auto context = UseEnvironment<DriveContext>();
  const auto entries = Project(context, folder, area);
  const bool selecting = !context.selected.Get().empty();
  View body;
  if (entries.empty()) body = EmptyFiles(area);
  else {
    const Id last = entries.back().id;
    const float bottom = floating ? 88.0F : 16.0F;
    if (context.grid.Get()) body = VirtualGrid(entries, [=](const Entry& item) {
      return Column{FileTile(item, true, true, area)}.With(Padding(EdgeInsets{.bottom = item.id == last ? bottom : 0})).Key(std::to_string(item.id));
    }).Columns(GridColumns::Adaptive(150)).ColumnSpacing(12).RowSpacing(12).With(Grow(), Padding(16)).Key("mobile-grid");
    else body = VirtualList(entries, [=](const Entry& item) {
      return Column{FileTile(item, false, true, area), Divider().With(Padding(EdgeInsets{.left = selecting ? 78.0F : 46.0F}))}
          .With(Padding(EdgeInsets{.bottom = item.id == last ? bottom : 0})).Key(std::to_string(item.id));
    }).EstimatedItemExtent(65).With(Grow(), Padding(EdgeInsets::Symmetric(16, 0)), Semantics{.role = SemanticRole::List, .label = app::strings::files}).Key("mobile-list");
  }
  if (floating) body = Stack{std::move(body), Column{Spacer(), Row{Spacer(), UploadButton(folder, true)}.With(Padding(16))}}.With(Grow());
  return body;
}

[[huxerui::composable]]
static View MobileWorkspace(Id folder, Area page_area) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  const auto sheets = UseBottomSheet();
  const auto searching = UseState(false);
  const Area area = folder ? Area::Files : page_area == Area::Files ? context.file_area.Get() : page_area;
  const auto entries = Project(context, folder, area);
  const auto* parent = Find(context.data.Get(), folder);
  const StringVariant title = parent ? StringVariant(parent->name) : StringVariant(AreaLabel(area));
  const bool selecting = !context.selected.Get().empty();
  const bool home = !folder && (area == Area::Files || area == Area::Recent || area == Area::Favorites);
  const bool listing = area != Area::Transfers && area != Area::Storage;
  const bool floating = area == Area::Files && !selecting && !searching.Get();
  const auto clear_search = [=] {
    searching = false; context.search = TextEditingValue::FromText(""); context.query = std::string{}; context.searching = false;
  };
  const auto select_all = [=] {
    std::vector<Id> ids; for (const auto& item : entries) ids.push_back(item.id); context.selected = std::move(ids);
  };
  Views header;
  Views bar;
  if (selecting) {
    bar.Add(IconButton(app::images::close, app::strings::clear).With(Frame{.width = 44, .height = 44})
        .OnClick([=] { context.selected = std::vector<Id>{}; }));
    bar.Add(SingleLine(StringVariant::Format(app::strings::selected, context.selected.Get().size()), 19, false, true).With(Grow()));
    bar.Add(ActionButton({}, app::strings::select_all, select_all, ActionStyle::Link)
        .With(Frame{.min_height = 44}, Enabled(context.selected.Get().size() < entries.size())));
  } else if (searching.Get()) {
    bar.Add(IconButton(app::images::back, app::strings::back).With(Frame{.width = 44, .height = 44}).OnClick(clear_search));
    bar.Add(DriveSearch().With(Grow()));
  } else {
    if (folder || area == Area::Trash || area == Area::Storage)
      bar.Add(IconButton(app::images::back, app::strings::back).With(Frame{.width = 44, .height = 44})
          .OnClick([=] { if (folder) GoBack(context); else GoArea(context, Area::Files); }));
    if (parent && area == Area::Files) bar.Add(FolderTitle(*parent));
    else bar.Add(SingleLine(home ? app::strings::files : AreaLabel(area), home ? 24 : 22, false, true).With(Grow()));
    if (home || area == Area::Transfers) bar.Add(PreferencesButton());
    else if (listing) {
      bar.Add(IconButton(app::images::search, app::strings::search).With(Frame{.width = 44, .height = 44}).OnClick([=] { searching = true; }));
      bar.Add(IconButton(app::images::more, app::strings::more).With(Frame{.width = 44, .height = 44}).OnClick([=] {
        std::vector<SheetAction> actions{{app::images::check, app::strings::select_all, select_all}};
        if (folder) {
          actions.push_back({app::images::info, app::strings::details, [=] { Navigate(context, folder, true); }});
          actions.push_back({app::images::edit, app::strings::rename, [=] { RenameDialog(context, folder, 0, false); }});
        }
        ShowActions(sheets, title, std::move(actions));
      }));
    }
  }
  header.Add(Row{std::move(bar)}.With(Frame{.height = 56}, Spacing(4), Padding(EdgeInsets::Symmetric(12, 0)), CrossAlign(CrossAxisAlignment::Center)));
  if (home && !selecting && !searching.Get()) {
    header.Add(Row{DriveSearch().With(Grow())}.With(Padding(EdgeInsets{.top = 4, .right = 16, .bottom = 12, .left = 16})));
    header.Add(Tabs({app::strings::all_short, app::strings::recent, app::strings::favorites},
        area == Area::Recent ? std::size_t{1} : area == Area::Favorites ? std::size_t{2} : std::size_t{0})
        .OnChanged([=](std::size_t index) { GoArea(context, index == 1 ? Area::Recent : index == 2 ? Area::Favorites : Area::Files); }));
  }
  if (listing) {
    header.Add(Row{Label(StringVariant::Format(app::strings::items, entries.size()), 12, true).With(Grow()),
        !home && !selecting ? Label(context.sort.Get() == 0 ? app::strings::name : context.sort.Get() == 1 ? app::strings::modified : app::strings::size, 12, true) : View(),
        SortButton(true).With(Enabled(!selecting)), ViewSwitch(true).With(Enabled(!selecting))}
        .With(Frame{.height = 44}, Padding(EdgeInsets::Symmetric(16, 0)), CrossAlign(CrossAxisAlignment::Center)));
  }
  if (context.searching.Get()) header.Add(ProgressBar().With(Frame{.height = 2}));
  header.Add(Divider());
  View body;
  if (area == Area::Transfers) body = TransferList().With(Padding(16));
  else if (area == Area::Storage) body = StorageSummary();
  else if (home) {
    std::vector<View> pages;
    for (const auto tab : {Area::Files, Area::Recent, Area::Favorites})
      pages.push_back(MobileFiles(0, tab, tab == Area::Files && !selecting && !searching.Get()).Key(static_cast<int>(tab)));
    body = Pager(std::move(pages), area == Area::Recent ? std::size_t{1} : area == Area::Favorites ? std::size_t{2} : std::size_t{0})
        .ScrollAxis(Axis::Horizontal).DragEnabled(!selecting && !searching.Get() && context.query.Get().empty())
        .OnChanged([=](std::size_t index) { GoArea(context, index == 1 ? Area::Recent : index == 2 ? Area::Favorites : Area::Files); })
        .With(Grow()).Key("file-tabs");
  } else body = MobileFiles(folder, area, floating);
  View result = Column{Column{std::move(header)}, std::move(body).With(Grow()), selecting ? SelectionBar() : View()}
      .With(Grow(), CrossAlign(CrossAxisAlignment::Stretch), Background(theme.colors.surface));
  if (selecting || searching.Get()) result = std::move(result).On<ViewEvents::BackRequested>([=] {
    if (!context.selected.Get().empty()) context.selected = std::vector<Id>{}; else clear_search();
  });
  return result;
}

[[huxerui::composable]]
View Workspace(Id folder, std::optional<Area> page_area) {
  const auto context = UseEnvironment<DriveContext>();
  const Area area = folder ? Area::Files : page_area.value_or(context.area.Get());
  if (area == Area::Settings) return SettingsPage();
  if (UseViewportClass() == ViewportClass::Compact) return MobileWorkspace(folder, area);
  const auto& theme = UseTheme();
  const auto hovering = UseState(false);
  const auto entries = Project(context, folder, area);
  const auto* parent = Find(context.data.Get(), folder);
  const StringVariant title = area == Area::Files && parent ? StringVariant(parent->name) : StringVariant(AreaLabel(area));
  const bool selecting = !context.selected.Get().empty();
  Views header;
  if (area == Area::Files) {
    Views breadcrumb;
    breadcrumb.Add(ActionButton({}, app::strings::files, [=] { GoArea(context, Area::Files); }, ActionStyle::Plain));
    if (parent) {
      std::vector<Entry> chain;
      for (auto* node = parent; node; node = Find(context.data.Get(), node->parent)) chain.push_back(*node);
      std::ranges::reverse(chain);
      for (const auto& item : chain) {
        breadcrumb.Add(Label("/", 13, true));
        breadcrumb.Add(ActionButton({}, item.name, [=] { context.area = Area::Files; context.path = NavigationPath<Route>{{item.id, false}}; context.selected = std::vector<Id>{}; }, ActionStyle::Plain));
      }
    }
    header.Add(ScrollView{Row{std::move(breadcrumb)}.With(Spacing(7), CrossAlign(CrossAxisAlignment::Center))}.With(Frame{.height = 26}));
  }
  Views title_row;
  title_row.Add(SingleLine(title, 30, false, true).With(Grow()));
  if (area == Area::Files) {
    title_row.Add(ActionButton(app::images::folder, app::strings::new_folder, [=] { RenameDialog(context, 0, folder, true); }));
    title_row.Add(UploadButton(folder, false).Key("desktop-upload"));
  }
  header.Add(Row{std::move(title_row)}.With(Spacing(10), CrossAlign(CrossAxisAlignment::Center), Frame{.min_height = 42}));
  if (area == Area::Transfers) return Column{Column{std::move(header)}.With(Spacing(12)), TransferList()}
      .With(Padding(28), Spacing(24), Grow(), CrossAlign(CrossAxisAlignment::Stretch));
  if (area == Area::Storage) return Column{Column{std::move(header)}.With(Spacing(12)), StorageSummary()}.With(Padding(28), Grow());
  header.Add(Label(StringVariant::Format(app::strings::items, entries.size()), 13, true));
  if (selecting) {
    const auto ids = context.selected.Get();
    Views actions;
    actions.Add(Label(StringVariant::Format(app::strings::selected, ids.size()), 13));
    actions.Add(IconButton(app::images::close, app::strings::clear).OnClick([=] { context.selected = std::vector<Id>{}; }));
    if (area == Area::Trash) {
      actions.Add(QuietButton(app::strings::restore, [=] { Change(context, [=](Snapshot& next) { return Trash(next, ids, true); }, [=] { context.selected = std::vector<Id>{}; }); }));
      actions.Add(IconButton(app::images::trash, app::strings::purge).OnClick([=] { ConfirmPurge(context, ids); }));
    } else {
      actions.Add(QuietButton(app::strings::move, [=] { DestinationDialog(context, ids, false); }));
      actions.Add(QuietButton(app::strings::copy, [=] { DestinationDialog(context, ids, true); }));
      actions.Add(IconButton(app::images::trash, app::strings::trash_action).OnClick([=] {
        Change(context, [=](Snapshot& next) { return Trash(next, ids, false); }, [=] { context.selected = std::vector<Id>{}; });
      }));
    }
    header.Add(Flow{std::move(actions)}.With(Spacing(10), CrossAlign(CrossAxisAlignment::Center)));
  } else {
    Views filters;
    const std::array<StringResource, 4> labels{app::strings::all_files, app::strings::documents, app::strings::images, app::strings::other};
    const std::array<std::optional<ImageResource>, 4> icons{{{}, app::images::file, app::images::image, app::images::file}};
    const TextStyle filter_style{Font::System(13).WithWeight(FontWeight::Medium), theme.colors.on_surface};
    float filter_width = 0;
    for (int i = 0; i < 4; ++i) filter_width = std::max(filter_width,
        UseTextMeasurer().MeasureText(UseString(labels[i]), filter_style).size.width + 28 + (icons[i] ? 26 : 0));
    for (int i = 0; i < 4; ++i) filters.Add(ActionButton(icons[i], labels[i], [=] { context.filter = i; },
        context.filter.Get() == i ? ActionStyle::Selected : ActionStyle::Outline).With(Frame{.width = filter_width}));
    if (UseViewportClass() != ViewportClass::Expanded) {
      header.Add(Flow{std::move(filters)}.With(Spacing(12)));
      filters = Views{};
    }
    filters.Add(Spacer());
    filters.Add(SortButton(false).Key("desktop-sort"));
    filters.Add(ViewSwitch(false));
    header.Add(Row{std::move(filters)}
        .With(Spacing(12), CrossAlign(CrossAxisAlignment::Center), Padding(EdgeInsets::Symmetric(0, 4))));
  }
  if (!context.grid.Get() && !entries.empty()) header.Add(Row{
      Checkbox(!entries.empty() && context.selected.Get().size() == entries.size()).With(Semantics{.label = app::strings::select_all}).OnChanged([=](bool checked) {
        std::vector<Id> ids; if (checked) for (auto& item : entries) ids.push_back(item.id); context.selected = std::move(ids);
      }), Row{}.With(Frame{.width = 34}), Label(app::strings::name, 12, true).With(Grow()),
      Label(app::strings::modified, 12, true).With(Frame{.width = 170}), Label(app::strings::size, 12, true).With(Frame{.width = 82}),
      Row{}.With(Frame{.width = 80})}.With(Spacing(16), Padding(EdgeInsets::Symmetric(4, 0)), Frame{.height = 34}, CrossAlign(CrossAxisAlignment::Center)));
  View files;
  if (entries.empty()) files = EmptyFiles(area);
  else if (context.grid.Get()) files = VirtualGrid(entries, [=](const Entry& item) { return FileTile(item, true, false, area).Key(std::to_string(item.id)); })
      .Columns(GridColumns::Adaptive(190)).ColumnSpacing(16).RowSpacing(16).With(Grow(), ScrollBar()).Key("file-grid");
  else files = VirtualList(entries, [=](const Entry& item) { return Column{FileTile(item, false, false, area), Divider()}.Key(std::to_string(item.id)); })
      .EstimatedItemExtent(57).With(Grow(), ScrollBar(), Semantics{.role = SemanticRole::List, .label = title}).Key("file-list");
  if (context.searching.Get()) header.Add(ProgressBar().With(Frame{.height = 2}));
  View content = Column{Column{std::move(header)}.With(Spacing(10)), Divider(), std::move(files)}
      .With(Padding(EdgeInsets{.top = 16, .right = 28, .bottom = 0, .left = 28}),
            Spacing(context.grid.Get() ? 16 : 0), Grow(), CrossAlign(CrossAxisAlignment::Stretch),
            Background(hovering.Get() ? theme.colors.secondary_container : theme.colors.surface));
  if (area == Area::Files) content = std::move(content).With(FileDropTarget::Accepts())
      .On<FileDropEvents::Entered>([=](const FileDropOffer&, const FileDropEvent&) { hovering = true; })
      .On<FileDropEvents::Exited>([=](const FileDropOffer&, const FileDropEvent&) { hovering = false; })
      .On<FileDropEvents::Dropped>([=](const std::vector<FileReference>& files, const FileDropEvent&) { ImportFiles(context, files, folder); })
      .On<FileDropEvents::Failed>([=](const IoError&, const FileDropEvent&) { context.error = Error::Io; });
  return content;
}

} // namespace huxer_drive
