#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <app_resources.h>
#include <huxerui/huxerui.h>

#include "music_motion.h"
#include "music_playback.h"
#include "spotlight_hover.h"

using namespace huxerui;

namespace huxer_music {
namespace {

bool MobileHost() {
#if defined(__ANDROID__)
  return true;
#else
  return false;
#endif
}

enum class AppPage { ForYou, Discover, Library };

Color White(float opacity = 1.0F) {
  return Color::Rgb(255, 255, 255, opacity);
}

Color Ink(float opacity = 1.0F) {
  return Color::Rgb(8, 11, 18, opacity);
}

Color Mix(Color from, Color to, float amount) {
  return {
      from.red + (to.red - from.red) * amount,
      from.green + (to.green - from.green) * amount,
      from.blue + (to.blue - from.blue) * amount,
      1.0F,
  };
}

ThemeDefinition MusicThemeDefinition(bool immersive_appearance) {
  ThemeSpec theme = FlatDarkThemeSpec();
  theme.colors.primary = immersive_appearance ? Color::Rgb(204, 190, 255) : Color::Rgb(170, 192, 220);
  theme.colors.on_primary = Color::Rgb(24, 19, 42);
  theme.colors.background = Color::Rgb(8, 11, 18);
  theme.colors.surface = Color::Rgb(19, 23, 34);
  theme.colors.surface_container_low = Color::Rgb(255, 255, 255, 0.045F);
  theme.colors.surface_container = Color::Rgb(255, 255, 255, 0.07F);
  theme.colors.on_surface = White();
  theme.colors.on_surface_variant = White(0.62F);
  theme.colors.outline = White(0.1F);
  theme.interactions.focus_ring = FocusRing{White(0.8F), 2.0F, 2.0F};
  theme.interactions.indication = Indication{
      .focus = IndicationLayer{.fill = White(0.08F)},
      .hover = IndicationLayer{.fill = White(0.07F)},
      .press = IndicationLayer{.fill = White(0.12F)},
  };

  ThemeDefinition definition = FlatThemeDefinition(theme);
  IconButtonStyle icons = IconButtonStyle::Default();
  icons.foreground = White(0.78F);
  icons.disabled_foreground = White(0.24F);
  icons.icon_size = 20.0F;
  icons.minimum_interactive_size = 42.0F;
  icons.state_layer_size = 38.0F;
  icons.corner_radius = 19.0F;
  icons.indication = theme.interactions.indication;
  definition.Set(icons);

  TextFieldStyle search = TextFieldStyle::Default();
  search.variant = TextFieldVariant::Outlined;
  search.show_label = false;
  search.outlined.background = White(0.045F);
  search.outlined.border = White(0.09F);
  search.outlined.hovered_border = White(0.18F);
  search.outlined.focused_border = theme.colors.primary;
  search.outlined.disabled_border = White(0.05F);
  search.outlined.minimum_height = 40.0F;
  search.text_style = TextStyle{Font::System(13.0F), White(0.9F)};
  search.placeholder_style = TextStyle{Font::System(12.5F), White(0.38F)};
  search.leading_icon = White(0.45F);
  search.focused_leading_icon = theme.colors.primary;
  search.selection = immersive_appearance ? Color::Rgb(140, 116, 255, 0.3F)
                                          : Color::Rgb(118, 150, 190, 0.28F);
  search.caret = theme.colors.primary;
  search.outlined.corner_radii = CornerRadii{20.0F};
  search.padding = EdgeInsets::Symmetric(13.0F, 8.0F);
  definition.Set(search);

  MenuStyle menu = MenuStyle::Default();
  menu.background = Color::Rgb(18, 22, 33, 0.98F);
  menu.foreground = White(0.84F);
  menu.icon_tint = White(0.58F);
  menu.item_indication = Indication{
      .focus = IndicationLayer{.fill = White(0.065F)},
      .hover = IndicationLayer{.fill = White(0.075F)},
      .press = IndicationLayer{.fill = White(0.11F)},
  };
  menu.separator_color = White(0.075F);
  menu.separator_mode = MenuSeparatorMode::BetweenSections;
  menu.content_padding = EdgeInsets::All(6.0F);
  menu.item_padding = EdgeInsets::Symmetric(12.0F, 8.0F);
  menu.minimum_width = 196.0F;
  menu.minimum_item_height = 38.0F;
  menu.corner_radii = CornerRadii{13.0F};
  menu.shadow = Shadow{Color::Rgb(0, 0, 0, 0.38F), {0.0F, 8.0F}, 24.0F, 0.0F};
  menu.motion = PresentationMotion{
      .initial_scale = 0.98F,
      .slide_distance = 7.0F,
      .enter = TweenSpec{0.18, Easing::EaseOut},
      .exit = TweenSpec{0.13, Easing::EaseInOut},
  };
  definition.Set(menu);

  ToastStyle toast = ToastStyle::Default();
  toast.background = Color::Rgb(22, 27, 40, 0.97F);
  toast.text_style = TextStyle{Font::System(12.5F).WithWeight(FontWeight::Medium), White(0.88F)};
  toast.padding = EdgeInsets::Symmetric(15.0F, 10.0F);
  toast.shadow = Shadow{Color::Rgb(0, 0, 0, 0.34F), {0.0F, 7.0F}, 22.0F, 0.0F};
  toast.corner_radii = CornerRadii{12.0F};
  toast.viewport_padding = EdgeInsets{16.0F, 16.0F, 22.0F, 236.0F};
  toast.motion = PresentationMotion{
      .initial_scale = 0.98F,
      .slide_distance = 8.0F,
      .enter = TweenSpec{0.2, Easing::EaseOut},
      .exit = TweenSpec{0.14, Easing::EaseInOut},
  };
  definition.Set(toast);
  return definition;
}

LinearGradient AmbientBackground(const Track& track, bool immersive_appearance) {
  if (!immersive_appearance) {
    return {
        .start = {0.0F, 0.0F},
        .end = {1.0F, 1.0F},
        .stops = {
            {0.0F, Mix(Color::Rgb(10, 14, 23), track.accent, 0.055F)},
            {0.46F, Color::Rgb(10, 14, 22)},
            {0.74F, Color::Rgb(7, 10, 17)},
            {1.0F, Color::Rgb(10, 11, 17)},
        },
    };
  }

  return {
      .start = {0.0F, 0.0F},
      .end = {1.0F, 1.0F},
      .stops = {
          {0.0F, Mix(track.background, track.accent, 0.18F)},
          {0.44F, track.background},
          {0.72F, Color::Rgb(8, 11, 18)},
          {1.0F, Mix(Color::Rgb(5, 7, 12), track.secondary, 0.1F)},
      },
  };
}

View BrandMark() {
  return Image(app::images::huxer_music).With(Frame{.width = 28.0F, .height = 28.0F});
}

View AccountPopupAction(std::string label, bool primary, std::function<void()> action) {
  return Row{
      Text(label).Style({
          Font::System(11.5F).WithWeight(FontWeight::SemiBold),
          primary ? White() : White(0.72F),
      }),
  }
      .OnClick(std::move(action))
      .With(
          Grow(),
          Frame{.height = 38.0F},
          MainAlign(MainAxisAlignment::Center),
          CrossAlign(CrossAxisAlignment::Center),
          Background(primary ? Color::Rgb(116, 91, 246, 0.82F) : White(0.055F)),
          Border{primary ? Color::Rgb(189, 174, 255, 0.22F) : White(0.065F), 1.0F},
          CornerRadius(10.0F),
          Indication{
              .hover = IndicationLayer{.fill = White(0.075F)},
              .press = IndicationLayer{.fill = White(0.12F)},
          },
          PointerCursor(PointerCursorKind::Hand),
          Semantics{.role = SemanticRole::Button, .label = label}
      );
}

[[huxerui::composable]]
View AccountPopup(PopupContext context, ToastHandle toast) {
  const State<bool> entered = UseState(false);
  Lifecycle([entered] { entered = true; });

  const auto show_feedback = [=](std::string message) {
    context.Dismiss();
    toast.Show(std::move(message));
  };

  return Column{
      Row{
          Row{Text("JW").Style({Font::System(13.0F).WithWeight(FontWeight::Bold), White()})}
              .With(
                  Frame{.width = 44.0F, .height = 44.0F},
                  MainAlign(MainAxisAlignment::Center),
                  CrossAlign(CrossAxisAlignment::Center),
                  Background(LinearGradient{
                      .start = {0.0F, 0.0F},
                      .end = {1.0F, 1.0F},
                      .stops = {
                          {0.0F, Color::Rgb(117, 92, 255)},
                          {1.0F, Color::Rgb(250, 102, 143)},
                      },
                  }),
                  CornerRadius(22.0F)
              ),
          Column{
              Text("Huxer Listener").Style({Font::System(13.0F).WithWeight(FontWeight::SemiBold), White(0.92F)}),
              Text("Local listening profile").Style({Font::System(10.5F), White(0.42F)}),
          }
              .With(Spacing(3.0F), Grow()),
          Text("FREE")
              .Style({Font::System(9.0F).WithWeight(FontWeight::Bold), Color::Rgb(208, 198, 255)})
              .With(
                  Padding(EdgeInsets::Symmetric(8.0F, 4.0F)),
                  Background(Color::Rgb(117, 92, 255, 0.13F)),
                  Border{Color::Rgb(156, 137, 255, 0.18F), 1.0F},
                  CornerRadius(8.0F)
              ),
      }
          .With(Spacing(11.0F), CrossAlign(CrossAxisAlignment::Center)),
      Text("Your listening profile is stored locally in this UI prototype.")
          .Style({Font::System(10.5F), White(0.42F)}),
      Row{
          AccountPopupAction("View profile", false, [=] { show_feedback("Profile preview is ready for a future account service."); }),
          AccountPopupAction("Upgrade", true, [=] { show_feedback("Huxer Plus is not connected in this prototype."); }),
      }
          .With(Spacing(8.0F)),
  }
      .With(
          Spacing(14.0F),
          Padding(16.0F),
          Frame{.width = 280.0F, .min_width = 280.0F, .max_width = 280.0F},
          Background(Color::Rgb(16, 20, 30, 0.985F)),
          Border{White(0.085F), 1.0F},
          CornerRadius(17.0F),
          Shadow{Color::Rgb(0, 0, 0, 0.42F), {0.0F, 10.0F}, 28.0F, 0.0F},
          Transition{AnimateTo(entered.Get() ? 1.0F : 0.0F, TweenSpec{0.19, Easing::EaseOut})}
              .Opacity(0.0F, 1.0F)
              .Offset({0.0F, 8.0F}, {})
      );
}

View MusicTitleBar() {
  return WindowTitleBar{
      Row{
          BrandMark(),
          Text(app::strings::app_name)
              .Style({Font::System(13.5F).WithWeight(FontWeight::SemiBold), White(0.9F)})
              .With(Offset(Point{0.0F, -0.5F})),
      }
          .With(
              Spacing(10.0F),
              Padding(EdgeInsets::Symmetric(16.0F, 0.0F)),
              CrossAlign(CrossAxisAlignment::Center),
              Frame{.height = 44.0F},
              Grow()
          ),
  }
      .With(
          Background(Ink(0.24F)),
          Border{White(0.065F), 1.0F}
      );
}

View NavItem(ImageVariant icon, StringVariant label, bool selected, std::function<void()> action) {
  return Row{
      Row{
          Image(std::move(icon)).Tint(selected ? White() : White(0.56F)).With(Frame{.width = 19.0F, .height = 19.0F}),
      }
          .With(
              Frame{.width = 22.0F, .min_width = 22.0F, .max_width = 22.0F},
              MainAlign(MainAxisAlignment::Center),
              CrossAlign(CrossAxisAlignment::Center)
          ),
      Text(label).Style({
          Font::System(14.0F).WithWeight(selected ? FontWeight::SemiBold : FontWeight::Regular),
          selected ? White() : White(0.56F),
      }),
  }
      .OnClick(std::move(action))
      .With(
          Spacing(12.0F),
          CrossAlign(CrossAxisAlignment::Center),
          Padding(EdgeInsets::Symmetric(13.0F, 0.0F)),
          Frame{.width = 184.0F, .height = 46.0F, .min_width = 184.0F, .max_width = 184.0F},
          Background(selected ? White(0.09F) : Color::Transparent()),
          CornerRadius(12.0F),
          Indication{
              .hover = IndicationLayer{.fill = White(0.06F)},
              .press = IndicationLayer{.fill = White(0.1F)},
          },
          PointerCursor(PointerCursorKind::Hand),
          Semantics{.role = SemanticRole::Button, .label = label, .selected = selected}
      );
}

View TrackRow(const Track& track, std::size_t index, bool selected, bool playing, std::function<void()> select) {
  return Row{
      Text(index < 9 ? "0" + std::to_string(index + 1) : std::to_string(index + 1))
          .Style({Font::System(11.0F).WithWeight(FontWeight::Medium), selected ? White(0.9F) : White(0.34F)}),
      Column{
          Text(std::string(track.title))
              .Style({Font::System(12.5F).WithWeight(FontWeight::SemiBold), selected ? White() : White(0.72F)}),
          Text(std::string(track.artist)).Style({Font::System(11.0F), White(0.38F)}),
      }
          .With(Spacing(2.0F), Grow()),
      selected
          ? View(Row{}.With(
                Frame{.width = 12.0F, .height = 16.0F},
                EqualizerMotion{.playing = playing, .accent = track.accent, .secondary = track.secondary}
            ))
          : View(Row{}.With(Frame{.width = 12.0F, .height = 16.0F})),
  }
      .OnClick(std::move(select))
      .With(
          Spacing(10.0F),
          CrossAlign(CrossAxisAlignment::Center),
          Padding(EdgeInsets::Symmetric(11.0F, 9.0F)),
          Background(selected ? White(0.075F) : Color::Transparent()),
          CornerRadius(10.0F),
          Indication{
              .hover = IndicationLayer{.fill = White(0.055F)},
              .press = IndicationLayer{.fill = White(0.09F)},
          },
          PointerCursor(PointerCursorKind::Hand),
          Semantics{
              .role = SemanticRole::ListItem,
              .label = std::string(track.title) + " by " + std::string(track.artist),
              .selected = selected,
          }
      );
}

[[huxerui::composable]]
View Sidebar(AppPage page, std::size_t selected_track, bool playing, State<bool> immersive_appearance,
             State<AppPage> page_state, State<TextEditingValue> search_state,
             std::function<void(std::size_t)> select_track) {
  const auto catalog = UseEnvironment<MusicContext>().tracks.Get();
  const PopupHandle account_popup = UsePopup();
  const MenuHandle user_menu = UseMenu();
  const ToastHandle toast = UseToast();
  const WindowHandle window = UseWindow();
  Views tracks;
  for (std::size_t index = 0; index < catalog.size(); ++index) {
    tracks.Add(TrackRow(catalog[index], index, index == selected_track, playing, [=] {
      select_track(index);
      page_state = AppPage::ForYou;
      search_state = TextEditingValue::FromText({});
    }));
  }

  const auto navigate = [=](AppPage destination) {
    page_state = destination;
    search_state = TextEditingValue::FromText({});
  };

  const auto show_account_popup = [=] {
    account_popup.Show(
        [toast](PopupContext context) { return AccountPopup(context, toast); },
        PopupOptions{
            .placement = {AnchorSide::Above, AnchorAlignment::Start},
            .gap = 11.0F,
            .viewport_margin = 12.0F,
        }
    );
  };

  const auto show_user_menu = [=] {
    user_menu.Show(
        {
            MenuItem("Settings", [toast] { toast.Show("Settings are mocked for this UI prototype."); }),
            MenuItem(
                "Appearance",
                std::vector<MenuEntry>{
                    MenuItem("Immersive glow", [=] {
                      immersive_appearance = true;
                      toast.Show("Immersive glow enabled.");
                    }).Checked(immersive_appearance.Get()),
                    MenuItem("Deep focus", [=] {
                      immersive_appearance = false;
                      toast.Show("Deep focus appearance enabled.");
                    }).Checked(!immersive_appearance.Get()),
                }
            ),
            MenuItem("About Huxer Music", [toast] { toast.Show("Huxer Music · Interface prototype", {2.6}); }),
            MenuSection{},
            MenuItem("Exit", [window] { window.Close(); }),
        },
        MenuOptions{
            .placement = {AnchorSide::Above, AnchorAlignment::End},
            .gap = 9.0F,
            .viewport_margin = 12.0F,
            .width = 206.0F,
        }
    );
  };

  return Column{
      Column{
          NavItem(app::images::home, app::strings::nav_for_you, page == AppPage::ForYou,
                  [=] { navigate(AppPage::ForYou); }),
          NavItem(app::images::discover, app::strings::nav_discover, page == AppPage::Discover,
                  [=] { navigate(AppPage::Discover); }),
          NavItem(app::images::library, app::strings::nav_library, page == AppPage::Library,
                  [=] { navigate(AppPage::Library); }),
      }
          .With(Spacing(4.0F)),
      Column{
          Text(app::strings::playlist_title)
              .Style({Font::System(12.0F).WithWeight(FontWeight::SemiBold), White(0.72F)}),
          Text(app::strings::playlist_subtitle).Style({Font::System(10.5F), White(0.36F)}),
          Column{std::move(tracks)}.With(Spacing(3.0F)),
      }
          .With(Spacing(8.0F)),
      Spacer().With(Grow()),
      Row{
          Row{
              Row{Text("JW").Style({Font::System(11.0F).WithWeight(FontWeight::Bold), White()})}
                  .With(
                      Frame{.width = 34.0F, .height = 34.0F},
                      MainAlign(MainAxisAlignment::Center),
                      CrossAlign(CrossAxisAlignment::Center),
                      Background(White(0.1F)),
                      CornerRadius(17.0F)
                  ),
              Column{
                  Text("Huxer Listener")
                      .Style({Font::System(12.0F).WithWeight(FontWeight::SemiBold), White(0.82F)}),
                  Text("Free session").Style({Font::System(10.5F), White(0.36F)}),
              }
                  .With(Spacing(2.0F), Grow()),
          }
              .OnClick(show_account_popup)
              .With(
                  account_popup.Anchor(),
                  Spacing(10.0F),
                  Grow(),
                  CrossAlign(CrossAxisAlignment::Center),
                  Padding(EdgeInsets::Symmetric(0.0F, 6.0F)),
                  CornerRadius(10.0F),
                  Indication{
                      .hover = IndicationLayer{.fill = White(0.055F)},
                      .press = IndicationLayer{.fill = White(0.09F)},
                  },
                  PointerCursor(PointerCursorKind::Hand),
                  Semantics{.role = SemanticRole::Button, .label = "Open profile"}
              ),
          Row{
              Image(app::images::more).Tint(White(0.42F)).With(Frame{.width = 18.0F, .height = 18.0F}),
          }
              .OnClick(show_user_menu)
              .With(
                  user_menu.Anchor(),
                  Frame{.width = 34.0F, .height = 34.0F},
                  MainAlign(MainAxisAlignment::Center),
                  CrossAlign(CrossAxisAlignment::Center),
                  CornerRadius(17.0F),
                  Indication{
                      .hover = IndicationLayer{.fill = White(0.065F)},
                      .press = IndicationLayer{.fill = White(0.1F)},
                  },
                  PointerCursor(PointerCursorKind::Hand),
                  Semantics{.role = SemanticRole::Button, .label = "Open user menu"}
              ),
      }
          .With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Center)),
  }
      .With(
          Spacing(30.0F),
          Padding(EdgeInsets{26.0F, 18.0F, 22.0F, 18.0F}),
          Frame{.width = 220.0F, .min_width = 220.0F, .max_width = 220.0F},
          Background(Ink(0.32F)),
          Border{White(0.07F), 1.0F}
      );
}

[[huxerui::composable]]
View LibraryMenu() {
  const auto music = UseEnvironment<MusicContext>();
  const auto menu = UseMenu();
  return IconButton(app::images::more, "Library options").With(menu.Anchor()).OnClick([=] {
    menu.Show({MenuItem("Open local audio", [music] { OpenAudio(music); })
        .Enabled(music.ready.Get() && !music.picking.Get())});
  });
}

[[huxerui::composable]]
View TopBar(State<TextEditingValue> search_state) {
  TextField search(search_state);
  search = std::move(search)
               .Label(app::strings::search)
               .Placeholder(app::strings::search_placeholder)
               .LeadingIcon(app::images::search)
               .InputConfiguration({
                   .type = TextInputType::Text,
                   .action = TextInputAction::Search,
               })
               .OnChanged([search_state](const TextEditingValue& value) { search_state = value; });

  return Row{
      std::move(search).With(Frame{.width = 320.0F, .height = 40.0F}),
      Spacer().With(Grow()),
  }
      .With(CrossAlign(CrossAxisAlignment::Center), Frame{.height = 44.0F});
}

View AlbumArtwork(const Track& track, bool playing, float artwork_size = 348.0F) {
  View artwork = Canvas([track](PaintContext& context, Size size) {
                   const Rect bounds{0.0F, 0.0F, size.width, size.height};
                   if (MobileHost()) {
                     context.DrawRect(bounds, Mix(track.accent, track.background, 0.3F), CornerRadii{28});
                     context.DrawCircle({size.width * 0.12F, size.height * 0.16F}, size.width * 0.65F,
                         Color{track.secondary.red, track.secondary.green, track.secondary.blue, 0.32F});
                   } else {
                   context.DrawRect(
                       bounds,
                       LinearGradient{
                           .start = {0.0F, 0.0F},
                           .end = {1.0F, 1.0F},
                           .stops = {
                               {0.0F, track.accent},
                               {0.52F, track.secondary},
                               {1.0F, Color::Rgb(18, 21, 32)},
                           },
                       },
                       CornerRadii{28.0F}
                   );
                   context.DrawRect(
                       bounds,
                       RadialGradient{
                           .center = {0.28F, 0.22F},
                           .radius = {0.72F, 0.72F},
                           .stops = {
                               {0.0F, White(0.52F)},
                               {0.28F, White(0.1F)},
                               {1.0F, Color::Transparent()},
                           },
                           .transform = {},
                       },
                       CornerRadii{28.0F}
                   );
                   }
                   context.DrawLine(
                       {size.width * 0.12F, size.height * 0.78F},
                       {size.width * 0.88F, size.height * 0.78F},
                       White(0.22F),
                       StrokeStyle{.width = 1.0F}
                   );
                 })
                     .With(
                         Frame{.width = artwork_size, .height = artwork_size},
                         Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch)
                     );

  return Stack{
      std::move(artwork),
      Row{}.With(
          Frame{.width = artwork_size, .height = artwork_size},
          Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch),
          AlbumMotion{
              .playing = playing,
              .accent = track.accent,
              .secondary = track.secondary,
          }
      ),
      Column{
          Row{
              Text("HUXER MUSIC").Style({Font::System(10.0F).WithWeight(FontWeight::Bold), White(0.76F)}),
              Spacer().With(Grow()),
              Text("AUDIO").Style({Font::System(10.0F).WithWeight(FontWeight::Medium), White(0.56F)}),
          },
          Spacer().With(Grow()),
          Text(std::string(track.collection))
              .Style({Font::System(12.0F).WithWeight(FontWeight::SemiBold), White(0.82F)}),
          Text("A study in color and quiet motion").Style({Font::System(10.5F), White(0.5F)}),
      }
          .With(Padding(24.0F), Spacing(5.0F), Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch)),
  }
      .With(
          Frame{.width = artwork_size, .height = artwork_size},
          CornerRadius(28.0F),
          ClipChildren(),
          Shadow(Ink(0.56F), {0.0F, 18.0F}, 42.0F, -4.0F),
          Border{White(0.13F), 1.0F},
          Scale(AnimateTo(playing ? 1.0F : 0.965F, TweenSpec{0.34, Easing::EaseOut}))
      );
}

View AlbumPanel(const Track& track, bool playing) {
  return Column{
      AlbumArtwork(track, playing),
      Column{
          Text(app::strings::now_playing)
              .Style({Font::System(10.5F).WithWeight(FontWeight::Bold), track.accent}),
          Text(std::string(track.title))
              .Style({Font::System(27.0F).WithWeight(FontWeight::Bold), White()}),
          Text(std::string(track.artist) + "  ·  " + std::string(track.collection))
              .Style({Font::System(13.0F).WithWeight(FontWeight::Medium), White(0.5F)}),
      }
          .With(Spacing(7.0F), CrossAlign(CrossAxisAlignment::Center)),
  }
      .With(Spacing(24.0F), CrossAlign(CrossAxisAlignment::Center), Frame{.width = 410.0F});
}

[[huxerui::composable]]
View LyricsPanel(const Track& track, bool compact = false) {
  static constexpr std::array<std::string_view, 7> lyrics{
      "Streetlights drift across the glass",
      "Another quiet hour moves past",
      "We let the city lose its name",
      "And leave our shadows in the rain",
      "Hold the moment, let it glow",
      "There is nowhere else to go",
      "Let the music carry us home",
  };
  const auto music = UseEnvironment<MusicContext>();
  const auto progress = music.player->Snapshot().progress;
  const double duration = progress.duration ? progress.duration->count() : 0.0;
  const int active = duration > 0 ? std::clamp(static_cast<int>(progress.position.count() / duration * lyrics.size()), 0, 6) : 0;
  const int count = compact ? 3 : 7;
  const int start = std::clamp(active - count / 2, 0, static_cast<int>(lyrics.size()) - count);
  Views lines;
  for (int index = start; index < start + count; ++index) {
    lines.Add(Text(std::string(lyrics[index])).Align(TextAlign::Center)
        .Style({Font::System(index == active ? (compact ? 18.0F : 22.0F) : (compact ? 13.0F : 15.0F))
            .WithWeight(index == active ? FontWeight::Bold : FontWeight::Medium),
            White(index == active ? 1.0F : 0.38F)})
        .With(Opacity(AnimateTo(index == active ? 1.0F : 0.7F, TweenSpec{0.25, Easing::EaseOut}))));
  }
  return Column{
      Row{Text(app::strings::lyrics).Style({Font::System(10).WithWeight(FontWeight::Bold), White(0.45F)}),
          Spacer().With(Grow()), Text("DEMO").Style({Font::System(9).WithWeight(FontWeight::Medium), track.accent})},
      Column{std::move(lines)}.With(Spacing(compact ? 14.0F : 24.0F), Grow(),
          MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Stretch)),
  }.With(Spacing(12), CrossAlign(CrossAxisAlignment::Stretch), Padding(compact ? 12.0F : 18.0F),
      Frame{.width = compact ? std::optional<float>{} : std::optional<float>{460.0F}, .height = compact ? 176.0F : 500.0F});
}

View ActionIcon(ImageVariant icon, StringVariant label, std::function<void()> action, Color tint = White(0.72F)) {
  return Row{
      Image(std::move(icon)).Tint(tint).With(Frame{.width = 20.0F, .height = 20.0F}),
  }
      .OnClick(std::move(action))
      .With(
          Frame{.width = 42.0F, .height = 42.0F},
          MainAlign(MainAxisAlignment::Center),
          CrossAlign(CrossAxisAlignment::Center),
          CornerRadius(21.0F),
          Indication{
              .hover = IndicationLayer{.fill = White(0.07F)},
              .press = IndicationLayer{.fill = White(0.13F)},
          },
          PointerCursor(PointerCursorKind::Hand),
          Semantics{.role = SemanticRole::Button, .label = label}
      );
}

View PlayButton(bool playing, std::function<void()> action) {
  const ImageVariant icon = playing ? ImageVariant(app::images::pause) : ImageVariant(app::images::play);
  const StringVariant label = playing ? StringVariant(app::strings::pause) : StringVariant(app::strings::play);
  return Row{
      Image(icon).Tint(White()).With(
          Frame{.width = 25.0F, .height = 25.0F}
      ),
  }
      .OnClick(std::move(action))
      .With(
          Frame{.width = 68.0F, .height = 68.0F},
          MainAlign(MainAxisAlignment::Center),
          CrossAlign(CrossAxisAlignment::Center),
          Background(Color::Rgb(31, 41, 55)),
          Border{White(0.12F), 1.0F},
          CornerRadius(34.0F),
          ClipChildren(),
          SpotlightHover{.radius = 90.0F, .corner_radius = 34.0F},
          PointerCursor(PointerCursorKind::Hand),
          Semantics{.role = SemanticRole::Button, .label = label}
      );
}

SliderStyle PlaybackSliderStyle(float width, Color active, float thumb_size) {
  SliderStyle style = SliderStyle::Default();
  style.width = width;
  style.height = 22.0F;
  style.track_height = 3.0F;
  style.inactive_track = White(0.13F);
  style.active_track = active;
  style.thumb = White(0.94F);
  style.stop_indicator = active;
  style.thumb_width = thumb_size;
  style.thumb_height = thumb_size;
  style.hovered_thumb_width = thumb_size + 3.0F;
  style.hovered_thumb_height = thumb_size + 3.0F;
  style.pressed_thumb_width = thumb_size + 5.0F;
  style.pressed_thumb_height = thumb_size + 5.0F;
  style.track_inside_corner_radius = 1.5F;
  style.animation_duration = 0.16;
  return style;
}

[[huxerui::composable]]
View PlaybackProgress(bool compact = false) {
  const auto music = UseEnvironment<MusicContext>();
  const auto snapshot = music.player->Snapshot();
  const auto draft = UseState(std::optional<float>{});
  const auto duration = snapshot.progress.duration;
  const bool seekable = duration && duration->count() > 0 && snapshot.seekability == MediaSeekability::Seekable;
  const float maximum = seekable ? static_cast<float>(duration->count()) : 1.0F;
  const float position = draft.Get().value_or(static_cast<float>(snapshot.progress.position.count()));
  auto slider = Slider(std::clamp(position, 0.0F, maximum)).Range(0, maximum)
      .OnChanged([draft, seekable](float value) { if (seekable) draft = value; })
      .OnCommitted([music, draft, seekable](float value) {
        if (seekable) (void)music.player->SeekTo(MediaTime{value});
        draft = std::optional<float>{};
      }).OnCanceled([draft]() { draft = std::optional<float>{}; });
  return Row{
      Text(TimeLabel(position)).Style({Font::System(11), White(0.6F)}).With(Frame{.width = 38.0F}),
      Row{ProvideEnvironment(PlaybackSliderStyle(compact ? 180.0F : 360.0F, White(0.78F), 8),
          std::move(slider).With(Enabled(seekable), Grow()))}.With(Grow()),
      Text(duration ? TimeLabel(duration->count()) : "--:--").Style({Font::System(11), White(0.6F)}).With(Frame{.width = 38.0F}),
  }.With(Spacing(8), CrossAlign(CrossAxisAlignment::Center));
}

[[huxerui::composable]]
View VolumeSlider() {
  const auto music = UseEnvironment<MusicContext>();
  return ProvideEnvironment(PlaybackSliderStyle(82, White(0.58F), 7),
      Slider(static_cast<float>(music.player->Snapshot().volume)).Range(0, 1).Step(0.01F)
          .OnChanged([music](float value) { (void)music.player->SetVolume(value); }));
}

[[huxerui::composable]]
View TransportControls(std::function<void(std::size_t)> select_track) {
  const auto music = UseEnvironment<MusicContext>();
  const auto state = music.player->Snapshot();
  const auto change = [=](int direction) {
    const auto count = music.tracks.Get().size();
    select_track((music.index.Get() + count + direction) % count);
  };
  return Row{
      ActionIcon(app::images::previous, app::strings::previous_track, [=] { change(-1); }),
      PlayButton(state.play_when_ready, [music] { TogglePlayback(music); }),
      ActionIcon(app::images::next, app::strings::next_track, [=] { change(1); }),
  }.With(Spacing(22), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center), Enabled(music.ready.Get()));
}

[[huxerui::composable]]
View PlayerDock(const Track& track, bool liked, State<bool> liked_state,
                std::function<void(std::size_t)> select_track, std::function<void()> open_queue) {
  const auto music = UseEnvironment<MusicContext>();
  return Row{
      Row{
          Stack{
              Canvas([track](PaintContext& context, Size size) {
                context.DrawRect(
                    {0.0F, 0.0F, size.width, size.height},
                    LinearGradient{
                        .start = {0.0F, 0.0F},
                        .end = {1.0F, 1.0F},
                        .stops = {{0.0F, track.accent}, {1.0F, track.secondary}},
                    },
                    CornerRadii{9.0F}
                );
                context.DrawCircle({size.width * 0.62F, size.height * 0.42F}, size.width * 0.24F, Ink(0.44F));
              }).With(
                  Frame{.width = 50.0F, .height = 50.0F},
                  Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch)
              ),
          }
              .With(Frame{.width = 50.0F, .height = 50.0F}, CornerRadius(9.0F), ClipChildren()),
          Column{
              Text(std::string(track.title))
                  .Style({Font::System(12.5F).WithWeight(FontWeight::SemiBold), White(0.92F)}),
              Text(std::string(track.artist)).Style({Font::System(11.0F), White(0.42F)}),
          }
              .With(Spacing(3.0F), Grow()),
          ActionIcon(
              liked ? ImageVariant(app::images::heart_filled) : ImageVariant(app::images::heart),
              liked ? StringVariant(app::strings::unlike) : StringVariant(app::strings::like),
              [liked_state] { liked_state = !liked_state.Get(); },
              liked ? track.secondary : White(0.6F)
          ),
      }
          .With(
              Spacing(11.0F),
              CrossAlign(CrossAxisAlignment::Center),
              Frame{.width = 240.0F, .min_width = 240.0F, .max_width = 240.0F}
          ),
      Column{
          TransportControls(select_track),
          PlaybackProgress().Key(static_cast<int>(music.index.Get())),
      }
          .With(Spacing(6.0F), Grow(), CrossAlign(CrossAxisAlignment::Center)),
      Row{
          Image(app::images::volume).Tint(White(0.52F)).With(Frame{.width = 18.0F, .height = 18.0F}),
          VolumeSlider(),
          ActionIcon(app::images::queue, app::strings::queue, open_queue),
      }
          .With(
              Spacing(10.0F),
              MainAlign(MainAxisAlignment::End),
              CrossAlign(CrossAxisAlignment::Center),
              Frame{.width = 200.0F, .min_width = 200.0F, .max_width = 200.0F}
          ),
  }
      .With(
          Spacing(14.0F),
          CrossAlign(CrossAxisAlignment::Center),
          Padding(EdgeInsets::Symmetric(18.0F, 10.0F)),
          Frame{.height = 108.0F},
          Background(Ink(0.46F)),
          Border{White(0.075F), 1.0F},
          CornerRadius(22.0F),
          Shadow(Ink(0.24F), {0.0F, 14.0F}, 32.0F, -10.0F)
      );
}

std::string Lowercase(std::string_view value) {
  std::string result(value);
  std::ranges::transform(result, result.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return result;
}

bool MatchesTrack(const Track& track, std::string_view query) {
  const std::string normalized = Lowercase(query);
  return Lowercase(track.title).find(normalized) != std::string::npos ||
         Lowercase(track.artist).find(normalized) != std::string::npos ||
         Lowercase(track.collection).find(normalized) != std::string::npos;
}

View ArtworkTile(const Track& track, float width, float height, float radius) {
  return Canvas([=](PaintContext& context, Size size) {
           const Rect bounds{0.0F, 0.0F, size.width, size.height};
           if (MobileHost()) {
             context.DrawRect(bounds, Mix(track.accent, track.secondary, 0.35F), CornerRadii{radius});
           } else {
           context.DrawRect(
               bounds,
               LinearGradient{
                   .start = {0.0F, 0.0F},
                   .end = {1.0F, 1.0F},
                   .stops = {
                       {0.0F, track.accent},
                       {0.58F, track.secondary},
                       {1.0F, Ink(0.8F)},
                   },
               },
               CornerRadii{radius}
           );
           context.DrawRect(
               bounds,
               RadialGradient{
                   .center = {0.24F, 0.2F},
                   .radius = {0.7F, 0.82F},
                   .stops = {
                       {0.0F, White(0.32F)},
                       {1.0F, Color::Transparent()},
                   },
               },
               CornerRadii{radius}
           );
           }
           context.DrawCircle({size.width * 0.67F, size.height * 0.44F}, std::min(size.width, size.height) * 0.23F,
                              Ink(0.48F));
           context.DrawCircle({size.width * 0.67F, size.height * 0.44F}, std::min(size.width, size.height) * 0.055F,
                              White(0.74F));
         })
      .With(Frame{.width = width, .height = height}, CornerRadius(radius), ClipChildren());
}

View PageHeading(std::string_view eyebrow, std::string_view title, std::string_view subtitle, Color accent) {
  return Column{
      Text(std::string(eyebrow)).Style({Font::System(10.0F).WithWeight(FontWeight::Bold), accent}),
      Text(std::string(title)).Style({Font::System(28.0F).WithWeight(FontWeight::Bold), White()}),
      Text(std::string(subtitle)).Style({Font::System(12.0F), White(0.42F)}),
  }
      .With(Spacing(5.0F));
}

View DiscoverHero(const Track& track, std::function<void()> action) {
  return Stack{
      Canvas([track](PaintContext& context, Size size) {
        const Rect bounds{0.0F, 0.0F, size.width, size.height};
        context.DrawRect(
            bounds,
            LinearGradient{
                .start = {0.0F, 0.0F},
                .end = {1.0F, 1.0F},
                .stops = {{0.0F, track.accent}, {0.52F, track.secondary}, {1.0F, Ink(0.9F)}},
            },
            CornerRadii{22.0F}
        );
        context.DrawCircle({size.width * 0.82F, size.height * 0.48F}, size.height * 0.58F, Ink(0.3F));
        context.DrawCircle({size.width * 0.82F, size.height * 0.48F}, size.height * 0.28F, White(0.11F));
        context.DrawCircle({size.width * 0.82F, size.height * 0.48F}, size.height * 0.07F, White(0.8F));
      }).With(Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch)),
      Column{
          Text("EDITOR'S CHOICE").Style({Font::System(10.0F).WithWeight(FontWeight::Bold), White(0.68F)}),
          Spacer().With(Grow()),
          Text(std::string(track.title)).Style({Font::System(25.0F).WithWeight(FontWeight::Bold), White()}),
          Text(std::string(track.artist) + " · " + std::string(track.collection))
              .Style({Font::System(12.0F).WithWeight(FontWeight::Medium), White(0.66F)}),
          Row{
              Image(app::images::play).Tint(Ink()).With(Frame{.width = 13.0F, .height = 13.0F}),
              Text("Play selection").Style({Font::System(11.0F).WithWeight(FontWeight::SemiBold), Ink()}),
          }
              .With(
                  Spacing(7.0F),
                  CrossAlign(CrossAxisAlignment::Center),
                  Padding(EdgeInsets::Symmetric(12.0F, 7.0F)),
                  Background(White(0.9F)),
                  CornerRadius(15.0F)
              ),
      }
          .With(Padding(22.0F), Spacing(8.0F), Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch)),
  }
      .OnClick(std::move(action))
      .With(
          Grow(),
          Frame{.height = 192.0F},
          Background(LinearGradient{
              .start = {0.0F, 0.0F},
              .end = {1.0F, 1.0F},
              .stops = {{0.0F, track.accent}, {0.52F, track.secondary}, {1.0F, Ink(0.9F)}},
          }),
          CornerRadius(22.0F),
          ClipChildren(),
          Indication{
              .hover = IndicationLayer{.fill = White(0.055F)},
              .press = IndicationLayer{.fill = Ink(0.12F)},
          },
          PointerCursor(PointerCursorKind::Hand),
          Semantics{.role = SemanticRole::Button, .label = std::string(track.title)}
      );
}

View MixCard(std::string_view label, std::string_view detail, Color accent, Color secondary) {
  return Column{
      Row{
          Row{}.With(Frame{.width = 8.0F, .height = 8.0F}, Background(accent), CornerRadius(4.0F)),
          Text("HUXER MIX").Style({Font::System(9.5F).WithWeight(FontWeight::Bold), White(0.45F)}),
      }
          .With(Spacing(7.0F), CrossAlign(CrossAxisAlignment::Center)),
      Spacer().With(Grow()),
      Text(std::string(label)).Style({Font::System(18.0F).WithWeight(FontWeight::Bold), White(0.92F)}),
      Text(std::string(detail)).Style({Font::System(11.0F), White(0.42F)}),
      Row{
          Row{}.With(Frame{.width = 34.0F, .height = 3.0F}, Background(accent), CornerRadius(1.5F)),
          Row{}.With(Frame{.width = 18.0F, .height = 3.0F}, Background(secondary), CornerRadius(1.5F)),
          Row{}.With(Frame{.width = 48.0F, .height = 3.0F}, Background(White(0.16F)), CornerRadius(1.5F)),
      }
          .With(Spacing(5.0F)),
  }
      .With(
          Padding(18.0F),
          Spacing(6.0F),
          Frame{.width = 252.0F, .height = 192.0F, .min_width = 252.0F, .max_width = 252.0F},
          Background(White(0.045F)),
          Border{White(0.075F), 1.0F},
          CornerRadius(22.0F)
      );
}

View SmallTrackCard(const Track& track, std::size_t index, std::function<void()> action) {
  return Row{
      ArtworkTile(track, 66.0F, 66.0F, 13.0F),
      Column{
          Text(std::string(track.title)).Style({Font::System(12.5F).WithWeight(FontWeight::SemiBold), White(0.9F)}),
          Text(std::string(track.artist)).Style({Font::System(10.5F), White(0.4F)}),
          Text(index == 0 ? "Dream pop" : index == 1 ? "Ambient focus" : "Electronic glow")
              .Style({Font::System(9.5F).WithWeight(FontWeight::Medium), track.accent}),
      }
          .With(Spacing(3.0F), Grow()),
      Image(app::images::play).Tint(White(0.62F)).With(Frame{.width = 15.0F, .height = 15.0F}),
  }
      .OnClick(std::move(action))
      .With(
          Spacing(11.0F),
          Grow(),
          CrossAlign(CrossAxisAlignment::Center),
          Padding(10.0F),
          Background(White(0.035F)),
          CornerRadius(16.0F),
          Indication{
              .hover = IndicationLayer{.fill = White(0.06F)},
              .press = IndicationLayer{.fill = White(0.1F)},
          },
          PointerCursor(PointerCursorKind::Hand),
          Semantics{.role = SemanticRole::Button, .label = std::string(track.title)}
      );
}

[[huxerui::composable]]
View DiscoverPage(std::function<void(std::size_t)> select_track) {
  const auto catalog = UseEnvironment<MusicContext>().tracks.Get();
  Views releases;
  for (std::size_t index = 0; index < std::min(std::size_t{3}, catalog.size()); ++index) {
    releases.Add(SmallTrackCard(catalog[index], index, [=] { select_track(index); }));
  }

  return Column{
      PageHeading("EXPLORE", "Discover", "New colors, late-night frequencies, and handpicked signals.", catalog[1].accent),
      Row{
          DiscoverHero(catalog[1], [=] { select_track(1); }),
          MixCard("Independent sounds", "Three songs by Josh Woodward · CC BY 4.0", catalog[1].accent,
                  catalog[0].secondary),
      }
          .With(Spacing(16.0F)),
      Row{
          Text("Offline collection").Style({Font::System(15.0F).WithWeight(FontWeight::SemiBold), White(0.86F)}),
          Spacer().With(Grow()),
          Text("Josh Woodward · CC BY 4.0").Style({Font::System(10.5F), White(0.35F)}),
      }
          .With(CrossAlign(CrossAxisAlignment::Center)),
      Row{std::move(releases)}.With(Spacing(12.0F)),
  }
      .With(Spacing(16.0F), Grow());
}

[[huxerui::composable]]
View LibraryTrackRow(const Track& track, std::size_t index, bool selected, std::function<void()> action) {
  const bool active = selected && UseEnvironment<MusicContext>().playing.Get();
  return Row{
      ArtworkTile(track, 48.0F, 48.0F, 10.0F),
      Column{
          Text(std::string(track.title))
              .Style({Font::System(12.5F).WithWeight(FontWeight::SemiBold), selected ? White() : White(0.82F)}),
          Text(std::string(track.artist) + " · " + std::string(track.collection))
              .Style({Font::System(10.5F), White(0.38F)}),
      }
          .With(Spacing(3.0F), Grow()),
      Text(std::string(track.duration)).Style({Font::System(10.5F).WithWeight(FontWeight::Medium), White(0.34F)}),
      Image(active ? ImageVariant(app::images::pause) : ImageVariant(app::images::play))
          .Tint(selected ? track.accent : White(0.48F))
          .With(Frame{.width = 15.0F, .height = 15.0F}),
  }
      .OnClick(std::move(action))
      .With(
          Spacing(12.0F),
          CrossAlign(CrossAxisAlignment::Center),
          Padding(EdgeInsets::Symmetric(10.0F, 8.0F)),
          Background(selected ? White(0.055F) : Color::Transparent()),
          CornerRadius(13.0F),
          Indication{
              .hover = IndicationLayer{.fill = White(0.05F)},
              .press = IndicationLayer{.fill = White(0.09F)},
          },
          PointerCursor(PointerCursorKind::Hand),
          Semantics{.role = SemanticRole::ListItem, .label = std::string(track.title), .selected = selected}
      );
}

View StatCard(std::string_view value, std::string_view label, Color accent) {
  return Row{
      Row{}.With(Frame{.width = 4.0F, .height = 30.0F}, Background(accent), CornerRadius(2.0F)),
      Column{
          Text(std::string(value)).Style({Font::System(17.0F).WithWeight(FontWeight::Bold), White(0.9F)}),
          Text(std::string(label)).Style({Font::System(10.0F), White(0.38F)}),
      }
          .With(Spacing(1.0F)),
  }
      .With(
          Spacing(11.0F),
          Grow(),
          CrossAlign(CrossAxisAlignment::Center),
          Padding(EdgeInsets::Symmetric(14.0F, 11.0F)),
          Background(White(0.035F)),
          Border{White(0.06F), 1.0F},
          CornerRadius(14.0F)
      );
}

[[huxerui::composable]]
View LibraryPage(std::size_t selected_track, bool liked, std::function<void(std::size_t)> select_track) {
  const auto catalog = UseEnvironment<MusicContext>().tracks.Get();
  Views tracks;
  for (std::size_t index = 0; index < catalog.size(); ++index) {
    tracks.Add(LibraryTrackRow(catalog[index], index, index == selected_track, [=] { select_track(index); }));
  }

  return Column{
      Row{PageHeading("YOUR COLLECTION", "Your Library", "Offline songs and local audio.", catalog[0].accent).With(Grow()), LibraryMenu()}
          .With(CrossAlign(CrossAxisAlignment::Center)),
      Row{
          StatCard(std::to_string(catalog.size()), "Tracks", catalog[0].accent),
          StatCard(liked ? "1" : "0", "Favorites", catalog[0].secondary),
          StatCard("3", "Offline songs", catalog[1].accent),
      }
          .With(Spacing(12.0F)),
      Row{
          Column{
              Row{
                  Text("All tracks").Style({Font::System(15.0F).WithWeight(FontWeight::SemiBold), White(0.86F)}),
                  Spacer().With(Grow()),
                  Text(std::to_string(catalog.size()) + " tracks").Style({Font::System(10.5F), White(0.34F)}),
              },
              Column{std::move(tracks)}.With(Spacing(3.0F)),
          }
              .With(Spacing(10.0F), Grow()),
          Column{
              Text("BUNDLED MUSIC").Style({Font::System(9.5F).WithWeight(FontWeight::Bold), catalog[2].accent}),
              Spacer().With(Grow()),
              Text("Josh Woodward").Style({Font::System(19.0F).WithWeight(FontWeight::Bold), White(0.92F)}),
              Text("3 songs · CC BY 4.0").Style({Font::System(10.5F), White(0.42F)}),
              Row{
                  Text("JW").Style({Font::System(10.0F).WithWeight(FontWeight::Bold), White()}),
              }
                  .With(
                      Frame{.width = 28.0F, .height = 28.0F},
                      MainAlign(MainAxisAlignment::Center),
                      CrossAlign(CrossAxisAlignment::Center),
                      Background(White(0.09F)),
                      CornerRadius(14.0F)
                  ),
          }
              .With(
                  Padding(18.0F),
                  Spacing(7.0F),
                  Frame{.width = 250.0F, .min_width = 250.0F, .max_width = 250.0F},
                  Background(LinearGradient{
                      .start = {0.0F, 0.0F},
                      .end = {1.0F, 1.0F},
                      .stops = {{0.0F, White(0.065F)}, {1.0F, Color::Rgb(244, 71, 134, 0.085F)}},
                  }),
                  Border{White(0.07F), 1.0F},
                  CornerRadius(18.0F)
              ),
      }
          .With(Spacing(18.0F), Grow()),
  }
      .With(Spacing(17.0F), Grow());
}

[[huxerui::composable]]
View SearchResultsPage(std::string_view query, std::size_t selected_track,
                       std::function<void(std::size_t)> select_track) {
  const auto catalog = UseEnvironment<MusicContext>().tracks.Get();
  Views results;
  std::size_t match_count = 0;
  for (std::size_t index = 0; index < catalog.size(); ++index) {
    if (!MatchesTrack(catalog[index], query)) {
      continue;
    }
    ++match_count;
    results.Add(LibraryTrackRow(catalog[index], index, index == selected_track, [=] { select_track(index); }));
  }

  View body = match_count > 0
                  ? View(Column{std::move(results)}.With(Spacing(5.0F)))
                  : View(Column{
                             Row{
                                 Image(app::images::search)
                                     .Tint(White(0.42F))
                                     .With(Frame{.width = 25.0F, .height = 25.0F}),
                             }
                                 .With(
                                     Frame{.width = 54.0F, .height = 54.0F},
                                     MainAlign(MainAxisAlignment::Center),
                                     CrossAlign(CrossAxisAlignment::Center),
                                     Background(White(0.055F)),
                                     CornerRadius(27.0F)
                                 ),
                             Text("No matching music")
                                 .Style({Font::System(17.0F).WithWeight(FontWeight::SemiBold), White(0.82F)}),
                             Text("Try a track, artist, or album name.").Style({Font::System(11.5F), White(0.38F)}),
                         }
                             .With(
                                 Spacing(8.0F),
                                 Grow(),
                                 MainAlign(MainAxisAlignment::Center),
                                 CrossAlign(CrossAxisAlignment::Center)
                             ));

  return Column{
      PageHeading("SEARCH", "Results for “" + std::string(query) + "”",
                  match_count == 1 ? "1 match in your Huxer Music catalog"
                                   : std::to_string(match_count) + " matches in your Huxer Music catalog",
                  catalog[0].accent),
      std::move(body).With(Grow()),
  }
      .With(Spacing(20.0F), Grow());
}

[[huxerui::composable]]
View MobileMusic() {
  const auto music = UseEnvironment<MusicContext>();
  const auto selected_page = UseState(std::size_t{0});
  const auto catalog = music.tracks.Get();
  const auto track = catalog[music.index.Get()];
  const auto select_track = [=](std::size_t index) {
    SelectTrack(music, index);
    selected_page = std::size_t{0};
  };
  Views rows;
  for (std::size_t index = 0; index < catalog.size(); ++index)
    rows.Add(LibraryTrackRow(catalog[index], index, music.index.Get() == index, [=] { select_track(index); }));
  std::vector<View> pages;
  pages.push_back(ScrollView{Column{
      AlbumArtwork(track, music.playing.Get(), 220),
      Column{Text(track.title).Align(TextAlign::Center).Style({Font::System(23).WithWeight(FontWeight::Bold), White()}),
          Text(track.artist).Style({Font::System(13), White(0.58F)})}
          .With(Spacing(7), CrossAlign(CrossAxisAlignment::Center)),
      LyricsPanel(track, true).With(Align(HorizontalAlignment::Stretch)),
  }.With(Spacing(18), Padding(EdgeInsets::Symmetric(20, 12)), CrossAlign(CrossAxisAlignment::Center))});
  pages.push_back(ScrollView{Column{
      Row{Text("Your library").Style({Font::System(24).WithWeight(FontWeight::Bold), White()}).With(Grow()), LibraryMenu()}
          .With(CrossAlign(CrossAxisAlignment::Center)),
      Text(std::to_string(catalog.size()) + " songs · ready to play").Style({Font::System(12), White(0.5F)}),
      Column{std::move(rows)}.With(Spacing(6)),
      Text("Bundled music by Josh Woodward\nGood to Go · On Brevity · Dizzy Spells\nCC BY 4.0 · joshwoodward.com\ncreativecommons.org/licenses/by/4.0/")
          .Style({Font::System(11), White(0.45F)}),
  }.With(Spacing(16), Padding(20), CrossAlign(CrossAxisAlignment::Stretch))});
  return Theme(MusicThemeDefinition(true), Column{
      Row{BrandMark(), Text("Huxer Music").Style({Font::System(17).WithWeight(FontWeight::Bold), White()}),
          Spacer().With(Grow())}
          .With(Spacing(10), Padding(EdgeInsets::Symmetric(20, 8)), CrossAlign(CrossAxisAlignment::Center)),
      Tabs({"Now playing", "Library"}, selected_page).OnChanged([=](std::size_t page) { selected_page = page; })
          .With(Padding(EdgeInsets::Symmetric(20, 0))),
      Pager(std::move(pages), selected_page.Get()).OnChanged([=](std::size_t page) { selected_page = page; }).With(Grow()),
      Column{PlaybackNotice(), PlaybackProgress(true).Key(static_cast<int>(music.index.Get())), TransportControls(select_track),
          Row{Image(app::images::volume).Tint(White(0.5F)).With(Frame{.width = 18.0F, .height = 18.0F}), VolumeSlider()}
              .With(Spacing(10), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center))}
          .With(Spacing(10), Padding(EdgeInsets{12, 24, 16, 24}), Background(Ink(0.38F))),
  }.With(CrossAlign(CrossAxisAlignment::Stretch), Background(track.background), SafeAreaPadding(),
      SystemBarsAppearance{.status_bar_background = track.background, .navigation_bar_background = track.background,
          .status_bar_content = SystemBarContentBrightness::Light, .navigation_bar_content = SystemBarContentBrightness::Light}));
}

[[huxerui::composable]]
View MusicShell() {
  if (MobileHost() || UseViewportClass() != ViewportClass::Expanded) return MobileMusic();

  const auto music = UseEnvironment<MusicContext>();
  const auto catalog = music.tracks.Get();
  const auto track_index = music.index;
  const auto playing = music.playing;
  const State<bool> liked = UseState(false);
  const State<bool> immersive_appearance = UseState(true);
  const State<AppPage> page = UseState(AppPage::ForYou);
  const State<TextEditingValue> search = UseState(TextEditingValue::FromText({}));
  const State<bool> entered = UseState(false);
  const SceneTransitionHandle transition = UseSceneTransition();
  const Track& track = catalog[track_index.Get()];

  Lifecycle([entered] { entered = true; });

  const auto select_track = [=](std::size_t index) {
    if (index >= catalog.size()) {
      return;
    }
    if (index == track_index.Get()) {
      (void)music.player->Play();
      page = AppPage::ForYou;
      search = TextEditingValue::FromText({});
      return;
    }
    transition.RunFromCurrentInteraction(
        CircularRevealSceneTransition{.animation = TweenSpec{0.42, Easing::EaseInOut}},
        [=] {
          SelectTrack(music, index);
          liked = false;
          page = AppPage::ForYou;
          search = TextEditingValue::FromText({});
        }
    );
  };

  View content;
  if (!search.Get().text.empty()) {
    content = SearchResultsPage(search.Get().text, track_index.Get(), select_track);
  } else {
    switch (page.Get()) {
      case AppPage::ForYou:
        content = Row{
                      AlbumPanel(track, playing.Get()),
                      LyricsPanel(track),
                  }
                      .With(
                          Spacing(36.0F),
                          Grow(),
                          Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch),
                          CrossAlign(CrossAxisAlignment::Center),
                          MainAlign(MainAxisAlignment::Center)
                      );
        break;
      case AppPage::Discover:
        content = DiscoverPage(select_track);
        break;
      case AppPage::Library:
        content = LibraryPage(track_index.Get(), liked.Get(), select_track);
        break;
    }
  }

  View shell = Row{
                   Sidebar(page.Get(), track_index.Get(), playing.Get(), immersive_appearance, page, search,
                           select_track),
                   Column{
                       TopBar(search),
                       PlaybackNotice(),
                       std::move(content).With(Grow()),
                       PlayerDock(track, liked.Get(), liked, select_track, [=] { page = AppPage::Library; search = TextEditingValue::FromText({}); }),
                   }
                       .With(
                           Spacing(18.0F),
                           Padding(EdgeInsets{20.0F, 30.0F, 24.0F, 30.0F}),
                           CrossAlign(CrossAxisAlignment::Stretch),
                           Grow()
                       ),
               }
                   .With(Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch));

  shell = std::move(shell).With(
      Transition{AnimateTo(entered.Get() ? 1.0F : 0.0F, TweenSpec{0.56, Easing::EaseOut})}
          .Opacity(0.0F, 1.0F)
          .Offset({0.0F, 14.0F}, {})
  );

  return Theme(
      MusicThemeDefinition(immersive_appearance.Get()),
      Column{
          MusicTitleBar(),
          Stack{
              std::move(shell),
          }
              .With(Grow(), ClipChildren()),
      }
          .With(
              CrossAlign(CrossAxisAlignment::Stretch),
              Background(AmbientBackground(track, immersive_appearance.Get())),
              AmbientMotion{
                  .playing = playing.Get() && immersive_appearance.Get(),
                  .accent = immersive_appearance.Get() ? track.accent : Color::Rgb(30, 38, 52),
                  .secondary = immersive_appearance.Get() ? track.secondary : Color::Rgb(25, 31, 44),
              },
              ClipChildren()
          )
  );
}

[[huxerui::composable]]
View HuxerMusicApp() {
  return ProvideEnvironment(UseMusic(), MusicShell());
}

AppOptions BuildOptions() {
  AppOptions options;
  options.window.title = "Huxer Music";
  options.window.initial_size = {1360.0F, 820.0F};
  options.window.minimum_size = Size{1280.0F, 720.0F};
  options.window.content_mode = WindowContentMode::SafeArea;
  options.window.chrome_mode = WindowChromeMode::Custom;
  options.window.title_bar_height = 44.0F;
  options.window.caption_labels = WindowCaptionLabels{
      .minimize = app::strings::window_minimize,
      .toggle_maximize = app::strings::window_toggle_maximize,
      .close = app::strings::window_close,
  };
  options.root_hooks = {huxerui::media::Install};
  options.show_debug_overlay = false;
  return options;
}

} // namespace
} // namespace huxer_music

const Application application{huxer_music::HuxerMusicApp, huxer_music::BuildOptions()};
