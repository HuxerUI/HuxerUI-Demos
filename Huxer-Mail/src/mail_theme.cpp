#include "mail_theme.h"

#include <huxerui/navigation.h>
#include <huxerui/presentation.h>

namespace huxer_mail {

using namespace huxerui;

CheckboxStyle MailCheckboxStyle(const ThemeSpec& theme) {
  CheckboxStyle checkbox = CheckboxStyle::Default();
  checkbox.checked_background = theme.colors.primary;
  checkbox.checkmark = theme.colors.on_primary;
  checkbox.unchecked_border = theme.colors.on_surface_variant;
  checkbox.disabled_checked_background = theme.colors.surface_container_highest;
  checkbox.disabled_checkmark = theme.colors.on_surface_variant;
  checkbox.disabled_unchecked_border = theme.colors.outline;
  checkbox.corner_radius = 5.0F;
  return checkbox;
}

ThemeDefinition MailThemeDefinition(ThemeMode mode, bool reduced_motion) {
  ThemeSpec theme = mode == ThemeMode::Dark ? FlatDarkThemeSpec() : FlatLightThemeSpec();
  if (mode == ThemeMode::Dark) {
    theme.colors.primary = Color::Rgb(157, 182, 225);
    theme.colors.on_primary = Color::Rgb(32, 45, 75);
    theme.colors.secondary = Color::Rgb(174, 183, 204);
    theme.colors.on_secondary = Color::Rgb(38, 43, 55);
    theme.colors.secondary_container = Color::Rgb(45, 58, 82);
    theme.colors.on_secondary_container = Color::Rgb(224, 231, 244);
    theme.colors.background = Color::Rgb(22, 24, 29);
    theme.colors.surface = Color::Rgb(32, 35, 41);
    theme.colors.surface_container_low = Color::Rgb(27, 30, 36);
    theme.colors.surface_container = Color::Rgb(39, 43, 51);
    theme.colors.surface_container_high = Color::Rgb(46, 51, 60);
    theme.colors.surface_container_highest = Color::Rgb(57, 64, 75);
    theme.colors.on_surface = Color::Rgb(242, 243, 246);
    theme.colors.on_surface_variant = Color::Rgb(180, 186, 198);
    theme.colors.outline = Color::Rgb(65, 72, 85);
    theme.colors.inverse_surface = Color::Rgb(239, 241, 245);
    theme.colors.inverse_on_surface = Color::Rgb(32, 35, 41);
    theme.colors.error = Color::Rgb(234, 146, 155);
  } else {
    theme.colors.primary = Color::Rgb(77, 112, 174);
    theme.colors.on_primary = Color::White();
    theme.colors.secondary = Color::Rgb(104, 116, 143);
    theme.colors.on_secondary = Color::White();
    theme.colors.secondary_container = Color::Rgb(232, 237, 247);
    theme.colors.on_secondary_container = Color::Rgb(48, 68, 108);
    theme.colors.background = Color::Rgb(247, 248, 250);
    theme.colors.surface = Color::White();
    theme.colors.surface_container_low = Color::Rgb(250, 250, 251);
    theme.colors.surface_container = Color::Rgb(243, 245, 248);
    theme.colors.surface_container_high = Color::Rgb(235, 238, 243);
    theme.colors.surface_container_highest = Color::Rgb(222, 226, 233);
    theme.colors.on_surface = Color::Rgb(25, 29, 37);
    theme.colors.on_surface_variant = Color::Rgb(101, 110, 126);
    theme.colors.outline = Color::Rgb(218, 223, 231);
    theme.colors.inverse_surface = Color::Rgb(30, 34, 43);
    theme.colors.inverse_on_surface = Color::Rgb(247, 248, 250);
    theme.colors.error = Color::Rgb(190, 82, 94);
  }
  theme.typography.body_large = 16.0F;
  theme.typography.body_medium = 14.0F;
  theme.typography.body_small = 12.0F;
  theme.typography.label_large = 14.0F;
  theme.typography.title_large = 20.0F;
  theme.typography.headline_small = 28.0F;
  theme.shapes.small = 10.0F;
  theme.shapes.medium = 14.0F;
  theme.shapes.large = 22.0F;
  theme.spacing.medium = 16.0F;
  theme.spacing.large = 24.0F;
  theme.motion.reduced_motion = reduced_motion;
  if (reduced_motion) {
    theme.motion.fast = 0.01;
    theme.motion.normal = 0.01;
    theme.motion.slow = 0.01;
  }
  theme.interactions.focus_ring = FocusRing{theme.colors.primary, 2.0F, 2.0F};
  const Color hover_fill =
      mode == ThemeMode::Dark ? Color::Rgb(157, 182, 225, 0.09F) : Color::Rgb(77, 112, 174, 0.06F);
  const Color press_fill = mode == ThemeMode::Dark ? Color::Rgb(157, 182, 225, 0.15F)
                                                   : Color::Rgb(77, 112, 174, 0.11F);
  theme.interactions.indication = Indication{
      .focus = IndicationLayer{.fill = hover_fill},
      .hover = IndicationLayer{.fill = hover_fill},
      .press = IndicationLayer{.fill = press_fill},
  };

  ThemeDefinition definition = FlatThemeDefinition(theme);
  ButtonStyle button = ButtonStyle::Default();
  button.background = theme.colors.primary;
  button.label_style = TextStyle{Font::System(14.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_primary};
  button.disabled_background = theme.colors.surface_container_high;
  button.disabled_label = theme.colors.on_surface_variant;
  button.padding = EdgeInsets::Symmetric(16.0F, 9.0F);
  button.minimum_height = 40.0F;
  button.corner_radius = 10.0F;
  definition.Set(button);

  IconButtonStyle icon_button = IconButtonStyle::Default();
  icon_button.foreground = theme.colors.on_surface_variant;
  icon_button.disabled_foreground = theme.colors.outline;
  icon_button.icon_size = 19.0F;
  icon_button.minimum_interactive_size = 40.0F;
  icon_button.state_layer_size = 34.0F;
  icon_button.corner_radius = 10.0F;
  definition.Set(icon_button);

  DividerStyle divider = DividerStyle::Default();
  divider.color = theme.colors.outline;
  definition.Set(divider);

  SegmentedButtonStyle segmented = SegmentedButtonStyle::Default();
  segmented.background = theme.colors.surface_container_low;
  segmented.selected_background = theme.colors.secondary_container;
  segmented.label_style =
      TextStyle{Font::System(13.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface_variant};
  segmented.selected_label = theme.colors.on_secondary_container;
  segmented.border = theme.colors.outline;
  segmented.selected_border = theme.colors.secondary_container;
  segmented.padding = EdgeInsets::Symmetric(13.0F, 7.0F);
  segmented.minimum_height = 34.0F;
  segmented.corner_radius = 10.0F;
  definition.Set(segmented);

  TextFieldStyle text_field = TextFieldStyle::Default();
  text_field.variant = TextFieldVariant::Outlined;
  text_field.show_label = false;
  text_field.outlined.background = theme.colors.surface;
  text_field.outlined.border = theme.colors.outline;
  text_field.outlined.hovered_border = theme.colors.on_surface_variant;
  text_field.outlined.focused_border = theme.colors.primary;
  text_field.outlined.disabled_border = theme.colors.surface_container_highest;
  text_field.outlined.minimum_height = 44.0F;
  text_field.text_style = TextStyle{Font::System(15.0F), theme.colors.on_surface};
  text_field.label_style =
      TextStyle{Font::System(13.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface_variant};
  text_field.floating_label_style =
      TextStyle{Font::System(12.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface_variant};
  text_field.placeholder_style = TextStyle{Font::System(14.0F), theme.colors.on_surface_variant};
  text_field.focused_label = theme.colors.primary;
  text_field.leading_icon = theme.colors.on_surface_variant;
  text_field.focused_leading_icon = theme.colors.primary;
  text_field.trailing_icon = theme.colors.on_surface_variant;
  text_field.focused_trailing_icon = theme.colors.primary;
  text_field.selection = mode == ThemeMode::Dark ? Color::Rgb(157, 182, 225, 0.26F)
                                                 : Color::Rgb(77, 112, 174, 0.20F);
  text_field.caret = theme.colors.primary;
  text_field.composition = theme.colors.primary;
  text_field.corner_radius = 12.0F;
  text_field.padding = EdgeInsets::Symmetric(12.0F, 10.0F);
  text_field.validation_error = theme.colors.error;
  text_field.error_label = theme.colors.error;
  text_field.validation_text_style = TextStyle{Font::System(12.0F), theme.colors.error};
  definition.Set(text_field);

  definition.Set(MailCheckboxStyle(theme));

  SwitchStyle toggle = SwitchStyle::Default();
  toggle.unchecked_track = theme.colors.surface_container_highest;
  toggle.checked_track = theme.colors.primary;
  toggle.unchecked_track_border = theme.colors.outline;
  toggle.track_border_width = 1.0F;
  definition.Set(toggle);

  ProgressCircleStyle progress = ProgressCircleStyle::Default();
  progress.track_color = theme.colors.surface_container_highest;
  progress.indeterminate_track_color = theme.colors.surface_container_highest;
  progress.indicator_color = theme.colors.primary;
  definition.Set(progress);

  TopAppBarStyle app_bar = TopAppBarStyle::Default();
  app_bar.background = theme.colors.surface;
  app_bar.title_style = TextStyle{Font::System(18.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface};
  app_bar.height = 58.0F;
  app_bar.horizontal_padding = 12.0F;
  app_bar.title_inset = 12.0F;
  app_bar.action_spacing = 2.0F;
  definition.Set(app_bar);

  NavigationPaneStyle pane = NavigationPaneStyle::Default();
  pane.background = Color::Transparent();
  pane.selected_content = theme.colors.primary;
  pane.indicator = theme.colors.secondary_container;
  pane.label_style = TextStyle{Font::System(14.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface_variant};
  pane.compact_width = 76.0F;
  pane.expanded_min_width = 232.0F;
  pane.item_margin = EdgeInsets::Symmetric(4.0F, 2.0F);
  pane.item_padding = EdgeInsets::Symmetric(12.0F, 0.0F);
  pane.item_height = 44.0F;
  pane.icon_size = 20.0F;
  pane.indicator_corner_radius = 12.0F;
  definition.Set(pane);

  DrawerStyle drawer = DrawerStyle::Default();
  drawer.background = theme.colors.surface_container_low;
  drawer.scrim = theme.colors.scrim;
  drawer.corner_radius = theme.shapes.large;
  definition.Set(drawer);

  ToastStyle toast = ToastStyle::Default();
  toast.background = theme.colors.inverse_surface;
  toast.text_style = TextStyle{Font::System(14.0F).WithWeight(FontWeight::Medium), theme.colors.inverse_on_surface};
  toast.corner_radius = theme.shapes.medium;
  definition.Set(toast);

  MenuStyle menu = MenuStyle::Default();
  menu.background = theme.colors.surface;
  menu.foreground = theme.colors.on_surface;
  menu.icon_tint = theme.colors.on_surface_variant;
  menu.separator_color = theme.colors.outline;
  menu.separator_mode = MenuSeparatorMode::BetweenSections;
  menu.content_padding = EdgeInsets{};
  menu.item_padding = EdgeInsets::Symmetric(14.0F, 9.0F);
  menu.minimum_item_height = 40.0F;
  menu.shadow = Shadow(Color::Rgb(0, 0, 0, mode == ThemeMode::Dark ? 0.28F : 0.12F), {0, 0}, 20, 0);
  menu.corner_radius = theme.shapes.medium;
  definition.Set(menu);

  DialogStyle dialog = DialogStyle::Default();
  dialog.background = theme.colors.surface;
  dialog.title_style = TextStyle{Font::System(20.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface};
  dialog.message_style = TextStyle{Font::System(14.0F), theme.colors.on_surface_variant};
  dialog.positive_action_background = theme.colors.primary;
  dialog.positive_action_style =
      TextStyle{Font::System(14.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_primary};
  dialog.negative_action_style =
      TextStyle{Font::System(14.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface_variant};
  dialog.action_separator_color = theme.colors.outline;
  dialog.action_corner_radius = 10.0F;
  dialog.corner_radius = theme.shapes.large;
  definition.Set(dialog);
  return definition;
}

} // namespace huxer_mail
