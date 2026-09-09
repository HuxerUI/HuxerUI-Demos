#include "drive_ui.h"

#include <ctime>
#include <algorithm>

namespace huxer_drive {

[[huxerui::composable]]
View SingleLine(StringVariant value, float size, bool muted, bool bold) {
  const auto& theme = UseTheme();
  const std::string text = UseString(value);
  const TextStyle style{Font::System(size).WithWeight(bold ? FontWeight::SemiBold : FontWeight::Regular),
                        muted ? theme.colors.on_surface_variant : theme.colors.on_surface};
  auto* measurer = &UseTextMeasurer();
  return Canvas([=](PaintContext& paint, Size bounds) {
    std::string visible = text;
    if (measurer->MeasureText(text, style).size.width > bounds.width) {
      std::vector<std::size_t> boundaries{0};
      for (std::size_t i = 1; i <= text.size(); ++i)
        if (i == text.size() || (static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) boundaries.push_back(i);
      std::size_t low = 0, high = boundaries.size() - 1;
      while (low < high) {
        const auto middle = (low + high + 1) / 2;
        if (measurer->MeasureText(text.substr(0, boundaries[middle]) + "…", style).size.width <= bounds.width) low = middle;
        else high = middle - 1;
      }
      visible = text.substr(0, boundaries[low]) + "…";
    }
    paint.DrawText({0, 0, bounds.width, bounds.height}, visible, style,
                   {.vertical_align = TextVerticalAlign::Center, .wrap = TextWrap::NoWrap});
  }).With(Frame{.height = size + 7}, ClipChildren(), Semantics{.label = value});
}

[[huxerui::composable]]
View MobileAction(ImageResource icon, StringVariant label, std::function<void()> action, bool destructive) {
  const auto& theme = UseTheme();
  const auto color = destructive ? theme.colors.error : theme.colors.on_surface;
  return Column{Glyph(icon, 23, color), Text(label).Style({Font::System(11), color}).Align(TextAlign::Center)}
      .With(Grow(), Frame{.min_height = 64}, Spacing(5), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center),
            Focusable{}, Semantics{.role = SemanticRole::Button, .label = label})
      .OnClick(action).On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
        if (event.key == Key::Enter || event.key == Key::Space) { action(); return true; } return false;
      });
}

[[huxerui::composable]]
static View ActionSheet(BottomSheetContext sheet, StringVariant title, std::vector<SheetAction> actions) {
  Views rows;
  rows.Add(Row{SingleLine(title, 18, false, true).With(Grow()),
      IconButton(app::images::close, app::strings::close).With(Frame{.width = 44, .height = 44}).OnClick([=] { (void)sheet.Dismiss(); })}
      .With(CrossAlign(CrossAxisAlignment::Center)));
  rows.Add(Divider());
  for (const auto& item : actions) {
    const auto click = [=] { (void)sheet.Dismiss(); item.action(); };
    rows.Add(Row{Glyph(item.icon, 22, item.destructive ? UseTheme().colors.error : UseTheme().colors.on_surface_variant),
        SingleLine(item.label).With(Grow())}
        .With(Spacing(16), Frame{.min_height = 52}, CrossAlign(CrossAxisAlignment::Center), Focusable{},
              Semantics{.role = SemanticRole::Button, .label = item.label})
        .OnClick(click).On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
          if (event.key == Key::Enter || event.key == Key::Space) { click(); return true; } return false;
        }));
  }
  return ScrollView{Column{std::move(rows)}.With(Padding(EdgeInsets::Symmetric(20, 12)), CrossAlign(CrossAxisAlignment::Stretch))}
      .With(Frame{.max_height = 620}, SafeAreaPadding{.top = false});
}

void ShowActions(BottomSheetHandle sheets, StringVariant title, std::vector<SheetAction> actions) {
  sheets.Show([=](BottomSheetContext sheet) { return ActionSheet(sheet, title, actions); });
}

[[huxerui::composable]]
View ActionButton(std::optional<ImageResource> icon, StringVariant text, std::function<void()> action,
                  ActionStyle style, bool chevron, float height) {
  const auto& theme = UseTheme();
  const auto hover = UseState(false);
  const bool primary = style == ActionStyle::Primary;
  const bool selected = style == ActionStyle::Selected;
  const bool plain = style == ActionStyle::Plain || style == ActionStyle::Link;
  const auto foreground = primary ? theme.colors.on_primary : selected || style == ActionStyle::Link ? theme.colors.primary : plain ? theme.colors.on_surface_variant : theme.colors.on_surface;
  const auto background = primary ? theme.colors.primary : selected ? theme.colors.secondary_container
      : hover.Get() ? theme.colors.surface_container : plain ? Color::Transparent() : theme.colors.surface;
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const float target_height = compact ? std::max(44.0F, height) : style == ActionStyle::Plain ? 26.0F : height;
  const TextStyle label_style{Font::System(13).WithWeight(FontWeight::Medium), foreground};
  const float width = UseTextMeasurer().MeasureText(UseString(text), label_style).size.width
      + (plain ? 6.0F : 26.0F) + (icon ? 26.0F : 0.0F) + (chevron ? 23.0F : 0.0F) + 2.0F;
  Views content;
  if (icon) content.Add(Glyph(*icon, 18, foreground));
  content.Add(Text(text).Style(label_style));
  if (chevron) content.Add(Glyph(app::images::chevron_down, 15, theme.colors.on_surface_variant));
  return Row{std::move(content)}
      .With(Spacing(8), CrossAlign(CrossAxisAlignment::Center), MainAlign(MainAxisAlignment::Center),
            Padding(EdgeInsets::Symmetric(plain ? 3 : 13, 0)), Frame{.width = width, .height = target_height},
            Background(background), Border{primary || selected || plain ? Color::Transparent() : theme.colors.outline, 1},
            CornerRadius(8), Focusable{}, Semantics{.role = SemanticRole::Button, .label = text, .selected = selected})
      .OnClick(action)
      .On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
        if (event.key == Key::Enter || event.key == Key::Space) { action(); return true; }
        return false;
      })
      .On<ViewEvents::Hover>([=](const HoverEvent& event) { hover = event.type != HoverEventType::Leave; });
}

[[huxerui::composable]]
View DialogActions(StringVariant confirm, std::function<void()> cancel, std::function<void()> submit, bool enabled) {
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const TextStyle style{Font::System(13).WithWeight(FontWeight::Medium), UseTheme().colors.on_surface};
  auto& measurer = UseTextMeasurer();
  const float width = std::max({88.0F, measurer.MeasureText(UseString(confirm), style).size.width + 28,
      measurer.MeasureText(UseString(app::strings::cancel), style).size.width + 28});
  View secondary = QuietButton(app::strings::cancel, std::move(cancel), 40);
  View primary = ActionButton({}, confirm, std::move(submit), ActionStyle::Primary, false, 40).With(Enabled(enabled));
  if (compact) {
    secondary = std::move(secondary).With(Grow());
    primary = std::move(primary).With(Grow());
  } else {
    secondary = std::move(secondary).With(Frame{.width = width});
    primary = std::move(primary).With(Frame{.width = width});
  }
  return Row{compact ? View() : Spacer(), std::move(secondary), std::move(primary)}
      .With(Spacing(12), CrossAlign(CrossAxisAlignment::Center));
}

[[huxerui::composable]]
View Avatar(bool small) {
  const auto& theme = UseTheme();
  return Text("AM").Style({Font::System(small ? 13 : 16), small ? theme.colors.primary : Color::White()})
      .Align(TextAlign::Center).VerticalAlign(TextVerticalAlign::Center)
      .With(Frame{.width = small ? 34.0F : 44.0F, .height = small ? 34.0F : 44.0F},
            Background(small ? theme.colors.secondary_container : theme.colors.primary), CornerRadius(24));
}

[[huxerui::composable]]
View PreferencesButton() {
  const auto context = UseEnvironment<DriveContext>();
  const auto sheets = UseBottomSheet();
  const auto action = [=] {
    ShowActions(sheets, app::strings::personal, {
      {app::images::settings, app::strings::settings, [=] { GoArea(context, Area::Settings); }},
      {app::images::trash, app::strings::trash, [=] { GoArea(context, Area::Trash); }},
      {app::images::disk, app::strings::storage, [=] { GoArea(context, Area::Storage); }},
      {app::images::sun, app::strings::theme, [=] { context.dark = !context.dark.Get(); }}
    });
  };
  return Row{Avatar(true)}.With(Frame{.width = 44, .height = 44}, MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center), Focusable{}, Semantics{.role = SemanticRole::Button, .label = app::strings::more})
      .OnClick(action).On<ViewEvents::KeyDown>([=](const KeyEvent& event) {
        if (event.key == Key::Enter || event.key == Key::Space) { action(); return true; } return false;
      });
}

[[huxerui::composable]]
View DriveSearch() {
  const auto context = UseEnvironment<DriveContext>();
  const auto generation = UseState(std::uint64_t{0});
  return TextField(context.search).Label(app::strings::search).Placeholder(app::strings::search).LeadingIcon(app::images::search)
      .With(Frame{.min_height = UseViewportClass() == ViewportClass::Compact ? 44.0F : 36.0F})
      .OnChanged([=](const TextEditingValue& value) {
        context.search = value;
        const auto serial = generation.Get() + 1; generation = serial; context.searching = true;
        (void)context.tasks.Launch([=]() -> Task<void> {
          co_await Delay(180ms);
          if (generation.Get() == serial && context.search.Get().text == value.text) {
            context.query = value.text; context.searching = false;
          }
        });
      });
}

[[huxerui::composable]]
View FileArtwork(Entry item, float size, bool thumbnail) {
  const auto context = UseEnvironment<DriveContext>();
  const auto raster = UseState(ImageAsset{});
  const auto tasks = UseTaskScope();
  const auto blob = item.versions.empty() ? 0 : item.versions.back().blob;
  const bool load = thumbnail && FileKind(item) == Kind::Image && !item.versions.empty() && item.versions.back().size <= 24 * 1024 * 1024;
  Lifecycle([=] {
    raster = ImageAsset{};
    const auto task = tasks.Launch([=]() -> Task<void> {
      if (!load) co_return;
      try {
        auto bytes = co_await context.store->Blob(blob).ReadBytesAsync();
        if (bytes.Succeeded()) raster = ImageAsset::FromEncoded(std::move(bytes).Value());
      } catch (...) {}
    });
    return [task] { task.Cancel(); };
  }, blob, load);
  if (raster.Get().HasValue()) return Image(raster.Get()).Fit(ImageFit::Cover)
      .With(Frame{.width = size, .height = size}, CornerRadius(5), ClipChildren());
  const auto name = Fold(item.name);
  ImageResource icon = app::images::file_text;
  if (item.folder) icon = app::images::folder_filled;
  else if (name.ends_with(".pdf")) icon = app::images::file_pdf;
  else if (name.ends_with(".xlsx") || name.ends_with(".csv")) icon = app::images::file_sheet;
  else if (name.ends_with(".zip") || name.ends_with(".7z")) icon = app::images::file_archive;
  else if (FileKind(item) == Kind::Image) return Glyph(app::images::image, size);
  return Image(icon).Fit(ImageFit::Contain).With(Frame{.width = size, .height = size});
}

StringVariant ShortDate(std::int64_t time) {
  const std::time_t now = Now(), value = time;
  const auto today = *std::localtime(&now);
  const auto date = *std::localtime(&value);
  const auto formatted = FormatDate(time);
  if (today.tm_year == date.tm_year && today.tm_yday == date.tm_yday)
    return StringVariant::Format(app::strings::today, formatted.substr(11, 5));
  const std::time_t previous = now - 86400;
  const auto yesterday = *std::localtime(&previous);
  if (yesterday.tm_year == date.tm_year && yesterday.tm_yday == date.tm_yday) return app::strings::yesterday;
  return formatted.substr(0, 10);
}

} // namespace huxer_drive
