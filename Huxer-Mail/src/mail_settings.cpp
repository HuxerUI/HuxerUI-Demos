#include "mail_views.h"

#include <app_resources.h>
#include <huxerui/huxerui.h>

#include "mail_context.h"

namespace huxer_mail {

using namespace huxerui;

[[huxerui::composable]]
View SettingsView(bool compact) {
  const AppStatusContext status = UseEnvironment<AppStatusContext>();
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const ThemeSpec& theme = UseTheme();
  const ToastHandle toast = UseToast();
  std::optional<View> leading = IconButton(app::images::back, app::strings::back).OnClick([navigation] {
    navigation.path = NavigationPath<MailRoute>{};
  });
  return Column{
      TopAppBar(app::strings::settings_title, std::move(leading)),
      ScrollView{
          Column{
              Column{
                  Text(app::strings::appearance_title, TextRole::Title)
                      .Style({Font::System(18.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
                  SegmentedButton(
                      std::vector<StringVariant>{app::strings::theme_light, app::strings::theme_dark},
                      status.theme.Get() == ThemeMode::Light ? 0 : 1
                  )
                      .OnChanged([status, toast](std::size_t index) {
                        const ThemeMode mode = index == 0 ? ThemeMode::Light : ThemeMode::Dark;
                        status.theme = mode;
                        toast.Show(
                            mode == ThemeMode::Light ? StringVariant(app::strings::theme_changed_light)
                                                     : StringVariant(app::strings::theme_changed_dark)
                        );
                      }),
              }
                  .With(
                      Spacing(16),
                      Padding(EdgeInsets::All(22)),
                      Background(theme.colors.surface),
                      CornerRadius(theme.shapes.medium)
                  ),
              Column{
                  Switch(app::strings::reduced_motion, status.reduced_motion).OnChanged([status, toast](bool enabled) {
                    status.reduced_motion = enabled;
                    toast.Show(
                        enabled ? StringVariant(app::strings::motion_changed_on)
                                : StringVariant(app::strings::motion_changed_off)
                    );
                  }),
                  Text(app::strings::reduced_motion_hint).Style({Font::System(13.0F), theme.colors.on_surface_variant}),
              }
                  .With(
                      Spacing(10),
                      Padding(EdgeInsets::All(22)),
                      Background(theme.colors.surface),
                      CornerRadius(theme.shapes.medium)
                  ),
              Column{
                  Text(app::strings::offline_title, TextRole::Title)
                      .Style({Font::System(18.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
                  Text(app::strings::offline_body).Style({Font::System(15.0F), theme.colors.on_surface_variant}),
              }
                  .With(
                      Spacing(10),
                      Padding(EdgeInsets::All(22)),
                      Background(theme.colors.surface),
                      CornerRadius(theme.shapes.medium)
                  ),
          }
              .With(
                  Frame{.max_width = 720.0F},
                  Spacing(14),
                  Padding(compact ? EdgeInsets::All(18) : EdgeInsets::All(30)),
                  Align(HorizontalAlignment::Center, VerticalAlignment::Start),
                  CrossAlign(CrossAxisAlignment::Stretch)
              ),
      }
          .With(ScrollBar(), Background(theme.colors.background), Grow()),
  }
      .With(Background(theme.colors.background), Grow());
}

} // namespace huxer_mail
