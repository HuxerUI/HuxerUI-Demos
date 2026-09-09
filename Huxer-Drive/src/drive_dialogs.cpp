#include "drive_ui.h"
#include <algorithm>

namespace huxer_drive {

[[huxerui::composable]]
static View NameEditor(DriveContext context, DialogContext dialog, Id id, Id parent, bool create) {
  const auto* item = Find(context.data.Get(), id);
  const auto name = UseState(TextEditingValue::FromText(item ? item->name : ""));
  const auto attempted = UseState(false);
  const bool valid = ValidName(name.Get().text);
  const auto submit = [=] {
    attempted = true;
    if (!ValidName(name.Get().text)) return;
    Change(context, [=](Snapshot& data) {
      return create ? CreateFolder(data, parent, name.Get().text) : Rename(data, id, name.Get().text);
    }, [=] { (void)dialog.Dismiss(); });
  };
  return Column{
      Label(create ? app::strings::new_folder : app::strings::rename, 22, false, true),
      Row{TextField(name).Label(app::strings::name).Placeholder(app::strings::name).MaxLength(120)
          .Validation(attempted.Get() && !valid ? ValidationResult::Invalid(app::strings::invalid_name) : ValidationResult::None())
          .OnChanged([=](const TextEditingValue& value) { name = value; }).OnSubmitted(submit).With(Grow())},
      context.error.Get() == Error::None ? View() : Label(ErrorLabel(context.error.Get()), 13),
      DialogActions(app::strings::save, [=] { (void)dialog.Dismiss(); }, submit, !context.busy.Get()),
  }.With(Padding(24), Spacing(18), Frame{.max_width = UseViewportClass() == ViewportClass::Compact ? 340.0F : 380.0F}, Background(UseTheme().colors.surface), CornerRadius(16));
}
void RenameDialog(const DriveContext& context, Id id, Id parent, bool create) {
  context.error = Error::None;
  context.dialogs->Show([=](DialogContext dialog) { return DriveDialog(context, NameEditor(context, dialog, id, parent, create)); });
}

[[huxerui::composable]]
static View DestinationPicker(DriveContext context, DialogContext dialog, std::vector<Id> ids, bool copy) {
  const auto folder = UseState(Id{0});
  const Snapshot data = context.data.Get();
  const auto* current = Find(data, folder.Get());
  const Id up = current ? current->parent : 0;
  Views choices;
  for (const auto& item : data.entries) {
    if (item.parent != folder.Get() || !item.folder || InTrash(data, item.id)) continue;
    if (std::ranges::any_of(ids, [&](Id id) { return Descendant(data, item.id, id); })) continue;
    choices.Add(Row{Glyph(app::images::folder, 22, Color::Rgb(242, 171, 48)), Label(item.name), Spacer(), Glyph(app::images::back)}
        .With(Padding(12), Spacing(10), CrossAlign(CrossAxisAlignment::Center)).OnClick([=] { folder = item.id; }));
  }
  return Column{
      Label(app::strings::destination, 22, false, true),
      Row{IconButton(app::images::back, app::strings::back).With(Enabled(folder.Get() != 0)).OnClick([=] { folder = up; }),
          Label(current ? StringVariant(current->name) : StringVariant(app::strings::files), 16, false, true)}.With(CrossAlign(CrossAxisAlignment::Center)),
      ScrollView{Column{std::move(choices)}}.With(Frame{.height = 230}, ScrollBar()),
      context.error.Get() == Error::None ? View() : Label(ErrorLabel(context.error.Get()), 13),
      DialogActions(copy ? app::strings::copy : app::strings::move, [=] { (void)dialog.Dismiss(); }, [=] {
            Change(context, [=](Snapshot& next) { return Relocate(next, ids, folder.Get(), copy); }, [=] {
              context.selected = std::vector<Id>{}; (void)dialog.Dismiss();
            });
          }, !context.busy.Get()),
  }.With(Padding(24), Spacing(16), Frame{.max_width = UseViewportClass() == ViewportClass::Compact ? 340.0F : 420.0F}, Background(UseTheme().colors.surface), CornerRadius(16));
}
void DestinationDialog(const DriveContext& context, std::vector<Id> ids, bool copy) {
  context.error = Error::None;
  context.dialogs->Show([=](DialogContext dialog) { return DriveDialog(context, DestinationPicker(context, dialog, ids, copy)); });
}

[[huxerui::composable]]
static View ShareEditor(DriveContext context, DialogContext dialog, Id id) {
  const Snapshot data = context.data.Get();
  const auto* found = Find(data, id);
  if (!found) return Label(app::strings::missing);
  const Entry item = *found;
  const auto days = UseState(std::size_t{0});
  const auto recipient = UseState(false);
  const auto unlocked = UseState(false);
  const auto attempted = UseState(false);
  const auto code = UseState(TextEditingValue::FromText(""));
  const bool valid = item.share && item.expires > Now() && !InTrash(data, id);
  Views contents;
  contents.Add(Label(item.name, 21, false, true));
  contents.Add(Label(app::strings::share_help, 13, true));
  if (recipient.Get()) {
    if (!valid) contents.Add(Label(app::strings::expired));
    else if (unlocked.Get()) {
      contents.Add(Label(app::strings::success, 15));
      if (!item.folder) contents.Add(Button(app::strings::download).OnClick([=] { Download(context, id); }));
      else contents.Add(QuietButton(app::strings::files, [=] { (void)dialog.Dismiss(); GoArea(context, Area::Files); Navigate(context, id, false); }, 40));
    } else {
      contents.Add(Row{TextField(code).Label(app::strings::code).Placeholder(app::strings::code)
          .OnChanged([=](const TextEditingValue& value) { code = value; })
          .Validation(attempted.Get() ? ValidationResult::Invalid(app::strings::invalid_code) : ValidationResult::None()).With(Grow())});
      contents.Add(Button(app::strings::unlock).OnClick([=] {
        const auto* current = Find(context.data.Get(), id);
        if (!current || current->share != item.share || current->expires <= Now()) { context.error = Error::Expired; return; }
        if (code.Get().text == current->code) unlocked = true; else attempted = true;
      }));
    }
  } else if (valid) {
    contents.Add(Label("drive-demo://share/" + std::to_string(item.share), 15));
    contents.Add(Row{Label(app::strings::code, 13, true), Label(item.code, 18, false, true)}.With(Spacing(12)));
    contents.Add(Row{Label(app::strings::expiry, 13, true), Label(FormatDate(item.expires), 13)}.With(Spacing(12)));
    contents.Add(Button(app::strings::share_preview).OnClick([=] { recipient = true; }));
    contents.Add(QuietButton(app::strings::unshare, [=] {
      Change(context, [=](Snapshot& next) {
        auto* current = Find(next, id); if (!current) return Error::Missing;
        current->share = 0; current->code.clear(); Record(*current, Action::Unshared); return Error::None;
      });
    }, 40));
  } else {
    contents.Add(Label(app::strings::expiry, 13, true));
    contents.Add(SegmentedButton({app::strings::days7, app::strings::days30}, days).OnChanged([=](std::size_t value) { days = value; }));
    contents.Add(Button(app::strings::create_share).With(Enabled(!context.busy.Get())).OnClick([=] {
      Change(context, [=](Snapshot& next) {
        auto* current = Find(next, id); if (!current || InTrash(next, id)) return Error::Missing;
        current->share = next.next_share++;
        current->code = std::to_string(100000 + (current->share * 7919) % 900000);
        current->expires = Now() + (days.Get() == 0 ? 7 : 30) * 86400;
        Record(*current, Action::Shared); return Error::None;
      });
    }));
  }
  contents.Add(QuietButton(app::strings::close, [=] { (void)dialog.Dismiss(); }, 40));
  return Column{std::move(contents)}.With(Padding(24), Spacing(18), Frame{.max_width = UseViewportClass() == ViewportClass::Compact ? 340.0F : 400.0F}, Background(UseTheme().colors.surface), CornerRadius(16));
}
void ShareDialog(const DriveContext& context, Id id) {
  context.dialogs->Show([=](DialogContext dialog) { return DriveDialog(context, ShareEditor(context, dialog, id)); });
}
void ConfirmPurge(const DriveContext& context, std::vector<Id> ids) {
  context.dialogs->Show(app::strings::purge, app::strings::purge_hint, app::strings::purge, app::strings::cancel, [=] {
    Run(context, [=]() -> Task<Error> {
      auto result = co_await context.store->Mutate([=](Snapshot& next) { return Purge(next, ids); });
      if (result != Error::None) co_return result;
      result = co_await context.store->Commit(context.store->Data());
      if (result != Error::None) co_return result;
      co_return co_await context.store->CollectGarbage();
    }, [=] { context.selected = std::vector<Id>{}; });
  });
}

} // namespace huxer_drive
