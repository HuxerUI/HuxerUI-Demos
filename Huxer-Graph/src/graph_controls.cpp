#include "graph_ui.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace graph::ui {
#if defined(_WIN32)
static constexpr bool ShowHistoryShortcuts = true;
#else
static constexpr bool ShowHistoryShortcuts = false;
#endif
Color CurveColor(int id, bool dark) {
  const Color colors[]{Color::Rgb(53, 95, 240), Color::Rgb(205, 75, 145), Color::Rgb(223, 139, 32), Color::Rgb(22, 131, 143)};
  const Color light[]{Color::Rgb(133, 159, 255), Color::Rgb(248, 136, 192), Color::Rgb(255, 191, 88), Color::Rgb(83, 204, 212)};
  return (dark ? light : colors)[id % 4];
}
[[huxerui::composable]]
View Label(StringVariant text, float size, bool muted, bool bold) {
  const auto& theme = UseTheme();
  return Text(text).Style({Font::System(size).WithWeight(bold ? FontWeight::SemiBold : FontWeight::Regular),
      muted ? theme.colors.on_surface_variant : theme.colors.on_surface});
}
[[huxerui::composable]]
View Icon(ImageResource image, StringVariant label, std::function<void()> action) {
  const float side = UseViewportClass() == ViewportClass::Compact ? 44 : 32;
  return IconButton(image, label).OnClick(action).With(Frame{.width = side, .height = side}, Tooltip(label));
}
[[huxerui::composable]]
View HistoryActions() {
  const auto c = UseEnvironment<Context>();
  (void)c.revision.Get();
  const auto action = [=](bool redo) {
    const auto label = redo ? app::strings::redo : app::strings::undo;
    const bool enabled = redo ? !c.history->redo.empty() : !c.history->undo.empty() || c.history->transaction.has_value();
    View button = Icon(redo ? app::images::redo : app::images::undo, label, [=] { Undo(c, redo); });
    if (ShowHistoryShortcuts) {
      button = View(button).With(Tooltip(UseString(label) + (redo ? " (Ctrl+Y / Ctrl+Shift+Z)" : " (Ctrl+Z)")));
    }
    return View(button).With(Enabled(enabled));
  };
  return Row {action(false), action(true)}.With(Spacing(4), CrossAlign(CrossAxisAlignment::Center));
}
[[huxerui::composable]]
View PrimaryButton(StringVariant label, std::function<void()> action) {
  const auto& t = UseTheme();
  auto button = UseEnvironment<ButtonStyle>();
  button.background = t.colors.primary;
  button.label_style = {Font::System(14).WithWeight(FontWeight::Medium), t.colors.on_primary};
  ThemeDefinition styles; styles.Set(button);
  return Theme(styles, Button(label).OnClick(action));
}
[[huxerui::composable]]
View Choice(StringVariant label, std::vector<StringVariant> items, std::size_t selected, std::function<void(std::size_t)> change) {
  return Column {
    Label(label, 12, true),
    Select(items, std::min(selected, items.size() - 1), [](const StringVariant& item) { return Text(item); })
        .Label(label).OnChanged(change).With(Frame{.min_height = UseViewportClass() == ViewportClass::Compact ? 44.0F : 34.0F}),
  }.With(Spacing(6), CrossAlign(CrossAxisAlignment::Stretch));
}
[[huxerui::composable]]
static View NumericDialog(DialogContext dialog, StringVariant label, double value, double min, double max, std::function<void(double)> change) {
  auto text = UseState(TextEditingValue::FromText(Number(value, 12)));
  auto error = UseState(false);
  const auto submit = [=] {
    std::istringstream stream(text.Get().text); stream.imbue(std::locale::classic()); double number;
    stream >> number; const bool parsed = !stream.fail(); stream >> std::ws;
    if (!parsed || !stream.eof() || !std::isfinite(number) || number < min || number > max) { error = true; return; }
    change(number); dialog.Dismiss();
  };
  return Column {
    Label(label, 20, false, true),
    TextField(text).Label(label).OnChanged([=](const TextEditingValue& value) { text = value; error = false; }).OnSubmitted(submit),
    Label(Number(min) + " … " + Number(max), 12, true),
    error.Get() ? Label(app::strings::failed, 13) : View(),
    Row {Button(app::strings::cancel).OnClick([=] { dialog.Dismiss(); }), Spacer(), PrimaryButton(app::strings::apply, submit)},
  }.With(Padding(24), Spacing(16), Frame{.max_width = 400}, CrossAlign(CrossAxisAlignment::Stretch),
      Background(UseTheme().colors.surface), CornerRadius(16));
}
[[huxerui::composable]]
View NumberControl(StringVariant label, double value, double min, double max, double step, std::function<void(double)> change) {
  const auto c = UseEnvironment<Context>();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const auto edit = [=] { c.dialogs->Show([=](DialogContext dialog) { return ProvideEnvironment(c, NumericDialog(dialog, label, value, min, max, change)); }); };
  View value_button = Button(Number(value)).OnClick(edit).With(Frame{.height = compact ? 44.0F : 32.0F, .min_width = 56});
  View slider = Slider(static_cast<float>(std::clamp(value, min, max))).Range(static_cast<float>(min), static_cast<float>(max)).Step(static_cast<float>(step))
        .OnStarted([=](float) { BeginEdit(c); }).OnChanged([=](float value) { change(value); })
        .OnCommitted([=](float value) { change(value); EndEdit(c); }).OnCanceled([=] { EndEdit(c, true); });
  if (!compact) return Row {
    Label(label, 12, true).With(Frame{.max_width = 92}), View(slider).With(Grow()), value_button,
  }.With(Spacing(8), CrossAlign(CrossAxisAlignment::Center));
  return Column {
    Row {Label(label, 13, true).With(Grow()), value_button}.With(Spacing(8), CrossAlign(CrossAxisAlignment::Center)),
    slider,
  }.With(Spacing(2), CrossAlign(CrossAxisAlignment::Stretch));
}
[[huxerui::composable]]
View FormulaField(StringVariant label, std::string value, std::function<void(std::string)> change) {
  const auto c = UseEnvironment<Context>();
  const auto focused = UseState(false);
  Lifecycle([=] { return [=] { if (focused.Get()) c.formula_focused = false; }; });
  auto editing = UseState(TextEditingValue::FromText(value));
  Lifecycle([=] { if (editing.Get().text != value) editing = TextEditingValue::FromText(value); }, value);
  auto style = UseEnvironment<TextFieldStyle>();
  style.show_label = false; style.variant = TextFieldVariant::Outlined;
  if (UseViewportClass() != ViewportClass::Compact) {
    style.outlined.minimum_height = 36; style.padding = EdgeInsets::Symmetric(10, 7);
  }
  ThemeDefinition overrides; overrides.Set(style);
  return Theme(overrides, TextField(editing).Label(label).MaxLength(2048)
      .OnChanged([=](const TextEditingValue& next) {
        editing = next;
        if (!next.composition) change(next.text);
      }).OnSubmitted([=] { EndEdit(c); })
      .On<ViewEvents::FocusChanged>([=](bool value) {
        focused = value; c.formula_focused = value;
        if (value) BeginEdit(c); else EndEdit(c);
      }));
}
} // namespace graph::ui
