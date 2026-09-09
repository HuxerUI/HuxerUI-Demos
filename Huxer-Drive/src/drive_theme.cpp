#include "drive_ui.h"

namespace huxer_drive {

[[huxerui::composable]]
View Label(StringVariant text, float size, bool muted, bool bold) {
  const auto& theme = UseTheme();
  return Text(std::move(text)).Style({Font::System(size).WithWeight(bold ? FontWeight::SemiBold : FontWeight::Regular),
                                   muted ? theme.colors.on_surface_variant : theme.colors.on_surface});
}
[[huxerui::composable]]
View Glyph(ImageResource icon, float size, std::optional<Color> tint) {
  return Image(icon).Tint(tint.value_or(UseTheme().colors.on_surface_variant)).With(Frame{.width = size, .height = size});
}
[[huxerui::composable]]
View QuietButton(StringVariant text, std::function<void()> action, float height) {
  return ActionButton({}, std::move(text), std::move(action), ActionStyle::Outline, false, height);
}
[[huxerui::composable]]
View DriveDialog(DriveContext context, View content) {
  return Theme(DriveTheme(context.dark.Get(), UseViewportClass() == ViewportClass::Compact), std::move(content));
}

ThemeDefinition DriveTheme(bool dark, bool compact) {
  ThemeSpec theme = dark ? FlatDarkThemeSpec() : FlatLightThemeSpec();
  theme.colors.primary = dark ? Color::Rgb(115, 160, 255) : Color::Rgb(5, 98, 255);
  theme.colors.on_primary = dark ? Color::Rgb(12, 24, 50) : Color::White();
  theme.colors.surface = dark ? Color::Rgb(23, 29, 42) : Color::White();
  theme.colors.background = theme.colors.surface;
  theme.colors.surface_container_low = dark ? Color::Rgb(19, 25, 37) : Color::Rgb(243, 247, 252);
  theme.colors.surface_container = dark ? Color::Rgb(30, 38, 53) : Color::Rgb(244, 247, 251);
  theme.colors.surface_container_high = dark ? Color::Rgb(36, 46, 62) : Color::Rgb(235, 240, 248);
  theme.colors.surface_container_highest = dark ? Color::Rgb(47, 60, 80) : Color::Rgb(226, 233, 243);
  theme.colors.secondary_container = dark ? Color::Rgb(35, 58, 96) : Color::Rgb(222, 235, 255);
  theme.colors.on_surface = dark ? Color::Rgb(236, 241, 249) : Color::Rgb(17, 29, 54);
  theme.colors.on_surface_variant = dark ? Color::Rgb(158, 174, 195) : Color::Rgb(98, 111, 133);
  theme.colors.outline = dark ? Color::Rgb(48, 59, 77) : Color::Rgb(227, 233, 241);
  theme.typography.body_medium = 14;
  theme.shapes.medium = 10;
  const auto hover = dark ? Color::Rgb(100, 155, 255, 0.09F) : Color::Rgb(49, 107, 250, 0.045F);
  theme.interactions.indication = Indication{.focus = IndicationLayer{.fill = hover}, .hover = IndicationLayer{.fill = hover},
                                           .press = IndicationLayer{.fill = Color::Rgb(49, 107, 250, 0.12F)}};
  theme.interactions.focus_ring = FocusRing{theme.colors.primary, 2, 2};
  auto definition = FlatThemeDefinition(theme);
  auto button = ButtonStyle::Default();
  button.background = theme.colors.primary;
  button.label_style = {Font::System(13).WithWeight(FontWeight::Medium), theme.colors.on_primary};
  button.minimum_height = compact ? 44 : 40;
  button.padding = EdgeInsets::Symmetric(13, 8);
  button.corner_radii = CornerRadii{8};
  button.indication = theme.interactions.indication;
  definition.Set(button);
  auto icon = IconButtonStyle::Default();
  icon.foreground = theme.colors.on_surface_variant;
  icon.minimum_interactive_size = compact ? 44 : 36;
  icon.icon_size = 18;
  icon.corner_radius = 8;
  icon.indication = theme.interactions.indication;
  definition.Set(icon);
  auto field = TextFieldStyle::Default();
  field.show_label = false;
  field.variant = TextFieldVariant::Outlined;
  field.outlined.background = dark ? theme.colors.surface_container : Color::Rgb(236, 240, 245);
  field.outlined.border = Color::Transparent();
  field.outlined.focused_border = theme.colors.primary;
  field.outlined.hovered_border = theme.colors.outline;
  field.outlined.corner_radii = CornerRadii{8};
  field.outlined.minimum_height = 36;
  field.text_style = {Font::System(14), theme.colors.on_surface};
  field.placeholder_style = {Font::System(14), theme.colors.on_surface_variant};
  field.caret = theme.colors.primary;
  field.padding = EdgeInsets::Symmetric(12, 7);
  definition.Set(field);
  auto divider = DividerStyle::Default(); divider.color = theme.colors.outline; definition.Set(divider);
  auto tabs = TabsStyle::Default();
  tabs.label_style = {Font::System(13), theme.colors.on_surface_variant};
  tabs.selected_label = theme.colors.primary; tabs.indicator = theme.colors.primary;
  tabs.indicator_sizing = TabIndicatorSizing::Content; tabs.indicator_min_width = 28;
  tabs.item_padding = EdgeInsets::Symmetric(16, 10); tabs.minimum_height = 44;
  definition.Set(tabs);
  auto checkbox = CheckboxStyle::Default(); checkbox.checked_background = theme.colors.primary; checkbox.checkmark = theme.colors.on_primary;
  checkbox.unchecked_border = {theme.colors.on_surface_variant, 1.4F}; checkbox.corner_radii = CornerRadii{3};
  checkbox.size = 18; checkbox.minimum_interactive_size = 20; definition.Set(checkbox);
  auto pane = NavigationPaneStyle::Default();
  pane.background = Color::Transparent(); pane.indicator = theme.colors.secondary_container;
  pane.selected_content = theme.colors.primary; pane.expanded_min_width = 208; pane.item_height = 46;
  pane.item_margin = EdgeInsets::Symmetric(12, 3); pane.indicator_corner_radius = 8;
  pane.label_style = {Font::System(14), theme.colors.on_surface_variant}; definition.Set(pane);
  auto menu = MenuStyle::Default(); menu.background = theme.colors.surface; menu.foreground = theme.colors.on_surface; definition.Set(menu);
  auto sheet = BottomSheetStyle::Default();
  sheet.background = theme.colors.surface;
  sheet.corner_radii = CornerRadii::Top(20);
  sheet.drag_handle = theme.colors.outline;
  sheet.drag_handle_size = {32, 4};
  sheet.drag_handle_padding = EdgeInsets::Symmetric(0, 8);
  definition.Set(sheet);
  auto bottom = NavigationBarStyle::Default();
  bottom.background = theme.colors.surface; bottom.indicator = Color::Transparent();
  bottom.selected_content = theme.colors.primary; bottom.label_style = {Font::System(11), theme.colors.on_surface_variant};
  bottom.icon_size = 23; bottom.height = 64; bottom.indication = theme.interactions.indication; definition.Set(bottom);
  auto progress = ProgressBarStyle::Default();
  progress.height = 5; progress.corner_radius = 3; progress.indicator_color = theme.colors.primary;
  progress.track_color = dark ? theme.colors.outline : Color::Rgb(216, 225, 238); definition.Set(progress);
  return definition;
}

} // namespace huxer_drive
