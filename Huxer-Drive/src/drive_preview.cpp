#include "drive_ui.h"

namespace huxer_drive {

static StringResource ActivityLabel(Action action) {
  switch (action) {
  case Action::Uploaded: return app::strings::upload;
  case Action::Renamed: return app::strings::rename;
  case Action::Moved: return app::strings::move;
  case Action::Copied: return app::strings::copy;
  case Action::Trashed: return app::strings::trash_action;
  case Action::Restored: return app::strings::restore;
  case Action::Shared: return app::strings::share;
  case Action::Unshared: return app::strings::unshare;
  case Action::VersionRestored: return app::strings::restore_version;
  default: return app::strings::new_folder;
  }
}
[[huxerui::composable]]
static View TextEditor(DriveContext context, DialogContext dialog, Id id, std::string original) {
  const auto editing = UseState(TextEditingValue::FromText(original));
  const auto discard = [=] {
    if (editing.Get().text == original) { (void)dialog.Dismiss(); return; }
    context.dialogs->Show(app::strings::close, app::strings::discard_hint, app::strings::confirm, app::strings::cancel,
        [=] { (void)dialog.Dismiss(); });
  };
  return Column{
      Row{Label(app::strings::edit, 22, false, true), Spacer(), IconButton(app::images::close, app::strings::close).OnClick(discard)}.With(CrossAlign(CrossAxisAlignment::Center)),
      Row{TextField(editing).Label(app::strings::edit).LineLimits(TextFieldLineLimits::MultiLine(6, 12))
          .OnChanged([=](const TextEditingValue& value) { editing = value; }).With(Grow())},
      context.error.Get() == Error::None ? View() : Label(ErrorLabel(context.error.Get()), 13),
      DialogActions(app::strings::save, discard, [=] {
        Run(context, [=] { return context.store->SaveText(id, editing.Get().text); }, [=] { (void)dialog.Dismiss(); });
      }, !context.busy.Get()),
  }.With(Padding(24), Spacing(16), Frame{.max_width = UseViewportClass() == ViewportClass::Compact ? 340.0F : 680.0F},
         Background(UseTheme().colors.surface), CornerRadius(16));
}

[[huxerui::composable]]
static View PreviewDetails(Id id) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  const auto tab = UseState(std::size_t{0});
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const auto* found = Find(context.data.Get(), id);
  if (!found) return Label(app::strings::missing);
  const Entry item = *found;
  Views info;
  info.Add(Tabs({app::strings::details_short, app::strings::versions, app::strings::activity}, tab).OnChanged([=](std::size_t index) { tab = index; }));
  if (tab.Get() == 0) {
    info.Add(Column{Label(app::strings::name, 12, true), Label(item.name, 15, false, true)}.With(Spacing(6)));
    info.Add(Column{Label(app::strings::modified, 12, true), Label(FormatDate(item.modified), 14)}.With(Spacing(6)));
    if (!item.folder) info.Add(Column{Label(app::strings::size, 12, true), Label(FormatSize(item.versions.back().size), 14)}.With(Spacing(6)));
    info.Add(Row{QuietButton(app::strings::rename, [=] { RenameDialog(context, id, 0, false); })});
  } else if (tab.Get() == 1) {
    if (!item.folder) info.Add(Button(app::strings::upload_version).OnClick([=] { Upload(context, item.parent, id); }));
    for (std::size_t index = item.versions.size(); index > 0; --index) {
      const Version version = item.versions[index - 1];
      Views buttons;
      buttons.Add(IconButton(app::images::download, app::strings::download)
          .With(Frame{.width = compact ? 44.0F : 36.0F, .height = compact ? 44.0F : 36.0F})
          .OnClick([=] { Download(context, id, version.blob); }));
      if (index != item.versions.size()) buttons.Add(QuietButton(app::strings::restore, [=] { Run(context, [=] { return context.store->RestoreVersion(id, version.blob); }); }));
      info.Add(Column{Label(StringVariant::Format(app::strings::version, index), 14, false, true),
          Row{Label(ShortDate(version.time), 12, true), Spacer(), Label(FormatSize(version.size), 12, true)}, Row{std::move(buttons)}.With(Spacing(8))}
          .With(Spacing(8), Padding(14), Background(theme.colors.surface), Border{theme.colors.outline, 1}, CornerRadius(8)));
    }
  } else {
    for (auto it = item.activity.rbegin(); it != item.activity.rend(); ++it)
      info.Add(Column{Label(ActivityLabel(it->action), 14), Label(FormatDate(it->time), 12, true)}.With(Spacing(6), Padding(8)));
  }
  return ScrollView{Column{std::move(info)}.With(Spacing(16), Padding(20), CrossAlign(CrossAxisAlignment::Stretch))}
      .With(ScrollBar(), Background(compact ? theme.colors.surface : theme.colors.surface_container_low),
            Border{compact ? Color::Transparent() : theme.colors.outline, 1}, CornerRadius(compact ? 0 : 12));
}

[[huxerui::composable]]
View Preview(Id id) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const Snapshot data = context.data.Get();
  const auto* found = Find(data, id);
  const auto text = UseState(std::string{});
  const auto image = UseState(ImageAsset{});
  const auto loading = UseState(true);
  const auto error = UseState(Error::None);
  const auto retry = UseState(0);
  const auto sheets = UseBottomSheet();
  const auto scope = UseTaskScope();
  const Id blob = found && !found->versions.empty() ? found->versions.back().blob : 0;
  const Kind kind = found ? FileKind(*found) : Kind::Other;
  Lifecycle([=] {
    loading = true; error = Error::None; image = ImageAsset{}; text = std::string{};
    const auto task = scope.Launch([=]() -> Task<void> {
      try {
        if (blob && (kind == Kind::Text || kind == Kind::Image)) {
          auto stat = co_await context.store->Blob(blob).StatAsync();
          if (!stat.Succeeded()) error = Error::Io;
          else if (stat.Value().size > (kind == Kind::Text ? 1024 * 1024 : 24 * 1024 * 1024)) error = Error::TooLarge;
          else if (kind == Kind::Text) {
            auto loaded = co_await context.store->Blob(blob).ReadStringAsync();
            if (loaded.Succeeded()) text = std::move(loaded).Value(); else error = Error::Io;
          } else {
            auto loaded = co_await context.store->Blob(blob).ReadBytesAsync();
            if (loaded.Succeeded()) image = ImageAsset::FromEncoded(std::move(loaded).Value()); else error = Error::Io;
          }
        }
      } catch (...) { error = Error::Io; }
      loading = false;
    });
    return [task] { task.Cancel(); };
  }, blob, kind, retry.Get());
  const auto back = [=] { GoBack(context); };
  if (!found || InTrash(data, id)) return Column{IconButton(app::images::back, app::strings::back).OnClick(back), Label(app::strings::missing)}.With(Padding(24), Grow());
  const Entry item = *found;
  Views actions;
  if (!item.folder) actions.Add(ActionButton({}, app::strings::download, [=] { Download(context, id); }, ActionStyle::Primary));
  actions.Add(QuietButton(app::strings::share, [=] { ShareDialog(context, id); }));
  if (kind == Kind::Text && error.Get() == Error::None && !loading.Get()) actions.Add(QuietButton(app::strings::edit, [=] {
    context.dialogs->Show([=](DialogContext dialog) { return DriveDialog(context, TextEditor(context, dialog, id, text.Get())); }, {.dismiss_on_outside_press = false, .dismiss_on_cancel = false});
  }));
  View preview;
  if (loading.Get()) preview = Column{ProgressCircle()}
      .With(Grow(), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center));
  else if (error.Get() != Error::None) preview = Column{
      Text(ErrorLabel(error.Get())).Style({Font::System(14), theme.colors.on_surface}).Align(TextAlign::Center),
      Button(app::strings::retry).OnClick([=] { retry += 1; })}
      .With(Grow(), Spacing(12), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center));
  else if (image.Get().HasValue()) preview = Image(image.Get()).Fit(ImageFit::Contain).With(Grow());
  else if (kind == Kind::Text) preview = ScrollView{Label(text.Get(), 15).With(Padding(compact ? 20 : 36))}.With(Grow(), ScrollBar(), Background(theme.colors.surface), Border{theme.colors.outline, 1}, CornerRadius(10));
  else preview = Column{Glyph(item.folder ? app::images::folder : app::images::file, 76, theme.colors.primary),
      Text(app::strings::no_preview).Style({Font::System(14), theme.colors.on_surface_variant}).Align(TextAlign::Center)}
      .With(Grow(), Spacing(22), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center));
  if (compact) {
    const auto details = [=] {
      sheets.Show([=](BottomSheetContext sheet) {
        return Column{
          Row{SingleLine(item.name, 18, false, true).With(Grow()),
            IconButton(app::images::close, app::strings::close).With(Frame{.width = 44, .height = 44}).OnClick([=] { (void)sheet.Dismiss(); })}
              .With(Padding(EdgeInsets::Symmetric(20, 4)), CrossAlign(CrossAxisAlignment::Center)),
          PreviewDetails(id).With(Frame{.height = 420})
        }.With(SafeAreaPadding{.top = false});
      });
    };
    const auto more = [=] {
      std::vector<SheetAction> choices{
        {app::images::edit, app::strings::rename, [=] { RenameDialog(context, id, 0, false); }},
        {app::images::star, app::strings::favorites, [=] { Change(context, [=](Snapshot& next) {
          auto* entry = Find(next, id); if (entry) entry->favorite = !entry->favorite; return Error::None;
        }); }},
        {app::images::folder, app::strings::move, [=] { DestinationDialog(context, {id}, false); }}
      };
      if (kind == Kind::Text && error.Get() == Error::None && !loading.Get())
        choices.insert(choices.begin(), {app::images::edit, app::strings::edit, [=] {
          context.dialogs->Show([=](DialogContext dialog) { return DriveDialog(context, TextEditor(context, dialog, id, text.Get())); },
              {.dismiss_on_outside_press = false, .dismiss_on_cancel = false});
        }});
      ShowActions(sheets, item.name, std::move(choices));
    };
    return Column{
      Row{IconButton(app::images::back, app::strings::back).With(Frame{.width = 44, .height = 44}).OnClick(back),
        SingleLine(item.name, 18, false, true).With(Grow()),
        IconButton(app::images::more, app::strings::more).With(Frame{.width = 44, .height = 44}).OnClick(more)}
          .With(Frame{.height = 56}, Padding(EdgeInsets::Symmetric(8, 0)), Spacing(8), CrossAlign(CrossAxisAlignment::Center)),
      Divider(),
      Column{std::move(preview).With(Grow())}.With(Grow(), Padding(12), Background(theme.colors.surface_container_low),
          MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Stretch)),
      Row{SingleLine(item.folder ? StringVariant(app::strings::new_folder) :
          StringVariant(FormatSize(item.versions.back().size) + " · " + UseString(ShortDate(item.modified))), 12, true).With(Grow())}
          .With(Frame{.height = 44}, Padding(EdgeInsets::Symmetric(20, 0)), CrossAlign(CrossAxisAlignment::Center)),
      Divider(),
      Row{MobileAction(app::images::share, app::strings::share, [=] { ShareDialog(context, id); }),
        !item.folder ? MobileAction(app::images::download, app::strings::download, [=] { Download(context, id); }) : View(),
        MobileAction(app::images::info, app::strings::details_short, details)}
          .With(Frame{.height = 64}, Padding(EdgeInsets::Symmetric(12, 0)), Enabled(!context.busy.Get()))
    }.With(Grow(), CrossAlign(CrossAxisAlignment::Stretch), Background(theme.colors.surface));
  }
  return Column{
    Row{IconButton(app::images::back, app::strings::back).OnClick(back), SingleLine(item.name, 25, false, true).With(Grow())}
        .With(Spacing(10), CrossAlign(CrossAxisAlignment::Center)),
    Flow{std::move(actions)}.With(Spacing(10)),
    Row{std::move(preview).With(Grow()), PreviewDetails(id).With(Frame{.width = 320})}
        .With(Spacing(24), CrossAlign(CrossAxisAlignment::Stretch), Grow())
  }.With(Padding(28), Spacing(18), Grow());
}

static StringResource TransferStatus(const Transfer& transfer) {
  if (transfer.running) return transfer.download_id ? app::strings::downloading : app::strings::uploading;
  if (transfer.completed) return app::strings::completed;
  return transfer.error == Error::None ? app::strings::transfer_waiting : app::strings::failed;
}

static std::string TransferProgress(const Transfer& transfer) {
  if (!transfer.total) return {};
  return transfer.completed ? FormatSize(transfer.total) : FormatSize(transfer.done) + " / " + FormatSize(transfer.total);
}

[[huxerui::composable]]
static View TransferDetails(DriveContext context, DialogContext dialog, Id id) {
  const auto transfers = context.transfers.Get();
  const Transfer* current = nullptr;
  for (const auto& transfer : transfers) if (transfer.id == id) { current = &transfer; break; }
  if (!current) return Label(app::strings::missing);
  const Transfer transfer = *current;
  Views rows;
  rows.Add(Label(app::strings::name, 12, true));
  rows.Add(Label(transfer.name, 15, false, true));
  rows.Add(Divider());
  rows.Add(Label(TransferStatus(transfer), 14));
  if (transfer.total) rows.Add(Label(TransferProgress(transfer), 13, true));
  if (transfer.error != Error::None) rows.Add(Label(transfer.download_id ? app::strings::export_hint : ErrorLabel(transfer.error), 14, true));
  return Column{
      Row{SingleLine(app::strings::transfer_details, 20, false, true).With(Grow()),
          IconButton(app::images::close, app::strings::close).OnClick([=] { (void)dialog.Dismiss(); })}
          .With(CrossAlign(CrossAxisAlignment::Center)),
      ScrollView{Column{std::move(rows)}.With(Spacing(12), CrossAlign(CrossAxisAlignment::Stretch))}
          .With(Frame{.max_height = 420}, ScrollBar())
  }.With(Padding(24), Spacing(16), CrossAlign(CrossAxisAlignment::Stretch),
         Frame{.max_width = UseViewportClass() == ViewportClass::Compact ? 340.0F : 460.0F},
         Background(UseTheme().colors.surface), CornerRadius(16));
}

[[huxerui::composable]]
View TransferList(bool compact_panel) {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  Views rows;
  auto transfers = context.transfers.Get();
  if (compact_panel && transfers.size() > 2) transfers.erase(transfers.begin(), transfers.end() - 2);
  for (const Transfer& transfer : transfers) {
    std::string status = UseString(TransferStatus(transfer));
    if (transfer.running && transfer.total) status += " · " + std::to_string(std::min<std::uint64_t>(99, transfer.done * 100 / transfer.total)) + "%";
    Entry artwork{.name = transfer.name};
    for (const auto& entry : context.data.Get().entries) if (entry.name == transfer.name) { artwork = entry; break; }
    const auto details = [=] {
      context.dialogs->Show([=](DialogContext dialog) { return DriveDialog(context, TransferDetails(context, dialog, transfer.id)); });
    };
    Views row;
    row.Add(Row{FileArtwork(artwork, 34),
        SingleLine(transfer.name, 14, false, true).With(Grow(), Frame{.min_height = 44}, Focusable{},
            Semantics{.role = SemanticRole::Button, .label = transfer.name}).OnClick(details)
            .On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
              if (event.key == Key::Enter || event.key == Key::Space) { details(); return true; } return false;
            }),
        transfer.error != Error::None ? ActionButton({}, app::strings::retry, [=] { RetryTransfer(context, transfer.id); }, ActionStyle::Selected)
                                     : transfer.completed ? Glyph(app::images::check, 17, theme.colors.primary) : View()}
        .With(Spacing(12), CrossAlign(CrossAxisAlignment::Center)));
    row.Add(Row{
        Text(status).Style({Font::System(12), transfer.error != Error::None ? theme.colors.error : theme.colors.on_surface_variant}),
        Spacer(), Label(TransferProgress(transfer), 12, true)}.With(Spacing(12), CrossAlign(CrossAxisAlignment::Center)));
    if (transfer.running) row.Add(Row{transfer.total ? View(ProgressBar(std::min(0.99F, static_cast<float>(transfer.done) / transfer.total)).With(Grow())) : View(ProgressBar().With(Grow()))});
    if (transfer.error != Error::None && !compact_panel) row.Add(Label(transfer.download_id ? app::strings::export_hint : ErrorLabel(transfer.error), 12, true));
    rows.Add(Column{std::move(row)}.With(Spacing(10), CrossAlign(CrossAxisAlignment::Stretch),
        Padding(EdgeInsets::Symmetric(compact_panel ? 0 : 16, 12)),
        Border{compact_panel ? Color::Transparent() : theme.colors.outline, 1}, CornerRadius(8)));
    if (compact_panel) rows.Add(Divider());
  }
  if (transfers.empty()) return Column{
      Glyph(app::images::transfer, 44, theme.colors.primary).With(Padding(18), Background(theme.colors.surface_container_low), CornerRadius(20)),
      Text(app::strings::transfer_empty).Style({Font::System(20).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}).Align(TextAlign::Center),
      Text(app::strings::transfer_empty_hint).Style({Font::System(14), theme.colors.on_surface_variant}).Align(TextAlign::Center)
          .With(Frame{.max_width = 360})}
      .With(Grow(), Padding(24), Spacing(14), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center));
  return compact_panel ? View(Column{std::move(rows)}.With(Spacing(12), CrossAlign(CrossAxisAlignment::Stretch)))
                       : View(ScrollView{Column{std::move(rows)}.With(Spacing(12), CrossAlign(CrossAxisAlignment::Stretch))}.With(Grow(), ScrollBar()));
}

} // namespace huxer_drive
