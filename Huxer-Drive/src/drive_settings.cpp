#include "drive_ui.h"

#include <array>

namespace huxer_drive {

[[huxerui::composable]]
static View SettingsLink(ImageResource icon, StringVariant title, StringVariant subtitle, std::function<void()> action) {
  return Row{Glyph(icon, 22),
      Column{SingleLine(title, 15, false, true), SingleLine(subtitle, 12, true)}
          .With(Grow(), Spacing(5), CrossAlign(CrossAxisAlignment::Stretch)),
      Glyph(app::images::chevron_right, 18)}
      .With(Frame{.min_height = 72}, Spacing(14), CrossAlign(CrossAxisAlignment::Center), Focusable{},
            Semantics{.role = SemanticRole::Button, .label = title})
      .OnClick(action).On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
        if (event.key == Key::Enter || event.key == Key::Space) { action(); return true; } return false;
      });
}

[[huxerui::composable]]
View SettingsPage() {
  const auto context = UseEnvironment<DriveContext>();
  const auto& theme = UseTheme();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const auto section = UseState(0);
  const auto back = [=] { if (section.Get()) section = 0; else GoArea(context, Area::Files); };
  const std::array<StringResource, 3> policies{app::strings::keep_both, app::strings::replace, app::strings::skip};
  const std::array<StringResource, 3> hints{app::strings::keep_both_hint, app::strings::replace_hint, app::strings::skip_hint};
  Views rows;
  if (section.Get() == 0) {
    rows.Add(SettingsLink(app::images::transfer, app::strings::transfers, policies[static_cast<int>(context.conflict.Get())], [=] { section = 1; }));
    rows.Add(Divider());
    rows.Add(SettingsLink(app::images::info, app::strings::demo_info, app::strings::demo_retry, [=] { section = 2; }));
  } else if (section.Get() == 1) {
    rows.Add(Label(app::strings::conflict_policy, 16, false, true));
    rows.Add(Label(app::strings::conflict_hint, 13, true));
    for (int index = 0; index < 3; ++index) {
      const auto select = [=] { context.conflict = static_cast<Conflict>(index); };
      rows.Add(Row{
          RadioButton(context.conflict.Get() == static_cast<Conflict>(index))
              .With(Frame{.width = 44, .height = 44}, Semantics{.label = policies[index]}).OnChanged([=](bool selected) { if (selected) select(); }),
          Column{Label(policies[index], 15, false, true), Label(hints[index], 13, true)}
              .With(Grow(), Spacing(6), CrossAlign(CrossAxisAlignment::Stretch))}
          .With(Padding(EdgeInsets::Symmetric(0, 12)), Spacing(8), CrossAlign(CrossAxisAlignment::Center))
          .OnClick(select));
      if (index < 2) rows.Add(Divider());
    }
  } else {
    rows.Add(Label(app::strings::demo_info_hint, 14, true));
    rows.Add(Label(app::strings::too_large, 14, true));
    rows.Add(Row{ActionButton(app::images::transfer, app::strings::demo_retry, [=] {
      auto values = context.transfers.Get();
      const Id id = values.empty() ? 1 : values.back().id + 1;
      values.push_back({.id = id, .name = "Quarterly planning and budget review - final presentation.pdf",
          .total = 5 * 1024 * 1024, .demonstration = true, .error = Error::DemoFailure});
      context.transfers = std::move(values);
      GoArea(context, Area::Transfers);
    }, ActionStyle::Outline).With(Enabled(!context.busy.Get()))});
  }
  return Column{
      Row{IconButton(app::images::back, app::strings::back).With(Frame{.width = 44, .height = 44}).OnClick(back),
          SingleLine(section.Get() == 1 ? app::strings::transfers : section.Get() == 2 ? app::strings::demo_info : app::strings::settings,
              compact ? 22 : 28, false, true).With(Grow())}
          .With(Frame{.height = compact ? 56.0F : 64.0F}, Padding(EdgeInsets::Symmetric(compact ? 8 : 20, 0)),
                Spacing(8), CrossAlign(CrossAxisAlignment::Center)),
      Divider(),
      ScrollView{Column{std::move(rows)}.With(Spacing(section.Get() == 1 ? 8 : 16),
          Padding(compact ? 20 : 28), Frame{.max_width = 600}, CrossAlign(CrossAxisAlignment::Stretch))}
          .With(Grow(), ScrollBar())
  }.With(Grow(), CrossAlign(CrossAxisAlignment::Stretch), Background(theme.colors.surface))
      .On<ViewEvents::BackRequested>(back);
}

} // namespace huxer_drive
