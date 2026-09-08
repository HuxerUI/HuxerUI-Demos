#include "mail_views.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string_view>

#include <app_resources.h>
#include <huxerui/huxerui.h>
#if defined(__EMSCRIPTEN__)
#include <huxerui/web/navigation.h>
#endif

#include "mail_context.h"
#include "mail_platform.h"
#include "mail_theme.h"
#include "mock_mail_service.h"

namespace huxer_mail {

using namespace huxerui;

namespace {

using namespace std::chrono_literals;

using MailNavigator = std::optional<RouteNavigationController<MailRoute>>;

StringResource FolderLabel(MailFolder folder) {
  switch (folder) {
  case MailFolder::Inbox:
    return app::strings::folder_inbox;
  case MailFolder::Starred:
    return app::strings::folder_starred;
  case MailFolder::Sent:
    return app::strings::folder_sent;
  case MailFolder::Drafts:
    return app::strings::folder_drafts;
  case MailFolder::Outbox:
    return app::strings::folder_outbox;
  case MailFolder::Trash:
    return app::strings::folder_trash;
  case MailFolder::Archive:
    return app::strings::folder_archive;
  }
  return app::strings::folder_inbox;
}

StringResource FilterLabel(MailFilter filter) {
  switch (filter) {
  case MailFilter::All:
    return app::strings::filter_all;
  case MailFilter::Unread:
    return app::strings::filter_unread;
  case MailFilter::Starred:
    return app::strings::filter_starred;
  }
  return app::strings::filter_all;
}

ImageResource FolderIcon(MailFolder folder) {
  switch (folder) {
  case MailFolder::Inbox:
    return app::images::inbox;
  case MailFolder::Starred:
    return app::images::star;
  case MailFolder::Sent:
    return app::images::sent;
  case MailFolder::Drafts:
    return app::images::draft;
  case MailFolder::Outbox:
    return app::images::outbox;
  case MailFolder::Trash:
    return app::images::trash;
  case MailFolder::Archive:
    return app::images::archive;
  }
  return app::images::inbox;
}

constexpr std::array<MailFolder, 6> kNavigationFolders{
    MailFolder::Inbox,
    MailFolder::Starred,
    MailFolder::Sent,
    MailFolder::Drafts,
    MailFolder::Outbox,
    MailFolder::Trash,
};

std::size_t FolderIndex(MailFolder folder) {
  const auto iterator = std::ranges::find(kNavigationFolders, folder);
  return iterator == kNavigationFolders.end() ? 0 : static_cast<std::size_t>(iterator - kNavigationFolders.begin());
}

std::string FirstGlyph(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  const unsigned char first = static_cast<unsigned char>(text.front());
  std::size_t length = 1;
  if ((first & 0xF8U) == 0xF0U) {
    length = 4;
  } else if ((first & 0xF0U) == 0xE0U) {
    length = 3;
  } else if ((first & 0xE0U) == 0xC0U) {
    length = 2;
  }
  return std::string(text.substr(0, std::min(length, text.size())));
}

Color AvatarColor(std::string_view id, bool dark) {
  constexpr std::array<Color, 5> light{
      Color::Rgb(227, 235, 247),
      Color::Rgb(235, 231, 244),
      Color::Rgb(229, 237, 232),
      Color::Rgb(241, 235, 227),
      Color::Rgb(242, 231, 234),
  };
  constexpr std::array<Color, 5> deep{
      Color::Rgb(48, 61, 82),
      Color::Rgb(61, 55, 76),
      Color::Rgb(50, 68, 60),
      Color::Rgb(72, 62, 52),
      Color::Rgb(72, 55, 62),
  };
  const std::size_t index = id.empty() ? 0 : static_cast<unsigned char>(id.back()) % light.size();
  return dark ? deep[index] : light[index];
}

std::size_t AttachmentCount(const MailThread& thread) {
  std::size_t count = 0;
  for (const MailMessage& message : thread.messages) {
    count += message.attachments.size();
  }
  return count;
}

std::string RelativeTime(
    int minutes,
    std::string now,
    std::string minute_text,
    std::string hour_text,
    std::string day_text
) {
  if (minutes < 1) {
    return now;
  }
  if (minutes < 60) {
    return minute_text;
  }
  if (minutes < 1440) {
    return hour_text;
  }
  return day_text;
}

StringResource SyncLabel(SyncPhase phase) {
  switch (phase) {
  case SyncPhase::Local:
    return app::strings::sync_local;
  case SyncPhase::Syncing:
    return app::strings::syncing;
  case SyncPhase::Synced:
    return app::strings::synced;
  case SyncPhase::Failed:
    return app::strings::sync_failed;
  }
  return app::strings::sync_local;
}

void ResetMailboxNavigation(
    const NavigationContext& navigation, const InteractionContext& interaction, MailFolder folder
) {
  navigation.folder = folder;
  navigation.path = NavigationPath<MailRoute>{};
  navigation.focused_thread = std::nullopt;
  navigation.drawer_open = false;
  navigation.filter = MailFilter::All;
  interaction.search = TextEditingValue::FromText({});
  interaction.search_open = false;
  interaction.search_results = std::vector<std::string>{};
  interaction.search_loading = false;
  ClearSelection(interaction);
}

void NavigateTo(MailRoute route, const NavigationContext& navigation, const MailNavigator& navigator) {
  if (route.kind == MailRouteKind::Reader) {
    navigation.focused_thread = route.value;
  }
  if (navigator) {
    navigator->Push(std::move(route));
  } else {
    navigation.path = NavigationPath<MailRoute>{std::move(route)};
  }
}

void BeginNewComposer(
    const MailboxDataContext& data,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    const MailNavigator& navigator
) {
  interaction.composer = ComposerDraft{
      .id = "compose-session-" + std::to_string(data.threads.Size() + data.drafts.Size() + 1),
  };
  interaction.composer_validation_attempted = false;
  interaction.attachment_errors = std::vector<std::string>{};
  NavigateTo({.kind = MailRouteKind::Composer}, navigation, navigator);
}

void PrepareComposer(
    const MailThread& thread,
    bool reply,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    const MailNavigator& navigator,
    std::string reply_subject
) {
  interaction.composer = ComposerDraft{
      .id = "compose-" + thread.id,
      .thread_id = reply ? std::optional<std::string>(thread.id) : std::nullopt,
      .recipient = thread.sender_email,
      .subject = reply ? std::move(reply_subject) : thread.subject,
      .body = reply                     ? "\n\n"
              : thread.messages.empty() ? std::string{}
                                        : thread.messages.back().body,
      .attachments =
          reply || thread.messages.empty() ? std::vector<MailAttachment>{} : thread.messages.back().attachments,
      .reply = reply,
  };
  interaction.composer_validation_attempted = false;
  interaction.attachment_errors = std::vector<std::string>{};
  NavigateTo({.kind = MailRouteKind::Composer, .value = thread.id}, navigation, navigator);
}

MailThread LocalizedSystemNotification(
    std::string sender,
    std::string email,
    std::string subject,
    std::string body,
    std::string recipient
) {
  return {
      .id = "system-refresh-thread",
      .subject = subject,
      .sender_name = sender,
      .sender_email = email,
      .excerpt = body,
      .relative_minutes = 0,
      .folder = MailFolder::Inbox,
      .unread = true,
      .messages = {{
          .id = "system-refresh-message",
          .sender_name = sender,
          .sender_email = email,
          .recipient_name = std::move(recipient),
          .recipient_email = "you@huxermail.test",
          .body = body,
      }},
  };
}

void StartRefresh(
    const MailboxDataContext& data,
    const AppStatusContext& status,
    const InteractionContext& interaction,
    const TaskScope& scope,
    const std::shared_ptr<MockMailService>& service,
    const ToastHandle& toast,
    MailThread notification,
    std::string live_started,
    std::string live_failed,
    std::string live_succeeded
) {
  const SyncStatus current = status.sync.Get();
  if (current.phase == SyncPhase::Syncing) {
    return;
  }
  const bool should_fail = !current.first_manual_attempt_consumed;
  const bool should_add_notification = !current.notification_added;
  status.sync.Update([should_fail](SyncStatus& value) {
    value.phase = SyncPhase::Syncing;
    value.retry_available = false;
    if (should_fail) {
      value.first_manual_attempt_consumed = true;
    }
  });
  interaction.live_announcement = std::move(live_started);
  (void)scope.Launch([=]() mutable -> Task<void> {
    const RefreshResult result = co_await service->Refresh(should_fail, should_add_notification);
    if (!result.succeeded) {
      status.sync.Update([](SyncStatus& value) {
        value.phase = SyncPhase::Failed;
        value.retry_available = true;
      });
      interaction.live_announcement = live_failed;
      toast.Show(app::strings::sync_failed);
      co_return;
    }
    status.sync.Update([&result](SyncStatus& value) {
      value.phase = SyncPhase::Synced;
      value.retry_available = false;
      if (result.should_add_notification) {
        value.notification_added = true;
      }
    });
    if (result.should_add_notification) {
      AddSystemNotification(data, std::move(notification));
      toast.Show(app::strings::sync_retry_success);
    } else {
      toast.Show(app::strings::synced);
    }
    interaction.live_announcement = live_succeeded;
  });
}

[[huxerui::composable]]
View NavigationPanel(bool expanded, ViewportClass viewport) {
  const MailboxDataContext data = UseEnvironment<MailboxDataContext>();
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const InteractionContext interaction = UseEnvironment<InteractionContext>();
  const AppStatusContext status = UseEnvironment<AppStatusContext>();
  const ThemeSpec& theme = UseTheme();
  const ToastHandle toast = UseToast();
  const bool dark = status.theme.Get() == ThemeMode::Dark;
  const auto toggle_theme = [status, toast, dark] {
    status.theme = dark ? ThemeMode::Light : ThemeMode::Dark;
    toast.Show(dark ? StringVariant(app::strings::theme_changed_light)
                    : StringVariant(app::strings::theme_changed_dark));
  };

  if (!expanded) {
    std::vector<NavigationItem> items;
    for (MailFolder folder : kNavigationFolders) {
      items.emplace_back(FolderIcon(folder), FolderLabel(folder));
    }
    return Column{
        NavigationPane(std::move(items), FolderIndex(navigation.folder.Get()), false)
            .OnChanged([navigation, interaction](std::size_t index) {
              if (index < kNavigationFolders.size()) {
                ResetMailboxNavigation(navigation, interaction, kNavigationFolders[index]);
              }
            })
            .With(Frame{.height = 276.0F}),
        Spacer(),
        IconButton(dark ? app::images::sun : app::images::moon,
                   dark ? StringVariant(app::strings::theme_light) : StringVariant(app::strings::theme_dark))
            .OnClick(toggle_theme)
            .With(
                Tooltip(dark ? StringVariant(app::strings::theme_light) : StringVariant(app::strings::theme_dark)),
                Align(HorizontalAlignment::Center, VerticalAlignment::Center)
            ),
    }
        .With(
            Background(theme.colors.background),
            Padding(EdgeInsets{12, 8, 14, 8}),
            Frame{.width = 68.0F}
        );
  }

  Views folder_rows;
  for (MailFolder folder : kNavigationFolders) {
    const bool selected = navigation.folder.Get() == folder;
    const std::size_t count = folder == MailFolder::Inbox ? InboxUnreadCount(data.threads)
                                                          : FolderCount(data.threads, folder);
    folder_rows.Add(
        Row{
            Row{}.With(
                Frame{.width = 3.0F, .height = 20.0F},
                Background(theme.colors.primary),
                CornerRadius(2.0F),
                Opacity(selected ? 1.0F : 0.0F)
            ),
            Image(FolderIcon(folder))
                .Tint(selected ? theme.colors.primary : theme.colors.on_surface_variant)
                .With(Frame{.width = 19.0F, .height = 19.0F}),
            Text(FolderLabel(folder))
                .Style({
                    Font::System(14.0F).WithWeight(selected ? FontWeight::SemiBold : FontWeight::Regular),
                    selected ? theme.colors.on_surface : theme.colors.on_surface_variant,
                })
                .With(Grow()),
            count > 0
                ? View(Text(std::to_string(count))
                           .Style({Font::System(12.0F).WithWeight(FontWeight::Medium),
                                   selected ? theme.colors.primary : theme.colors.on_surface_variant}))
                : View(Row{}.With(Frame{.width = 0.0F})),
        }
            .OnClick([navigation, interaction, folder] { ResetMailboxNavigation(navigation, interaction, folder); })
            .With(
                Spacing(12),
                Padding(EdgeInsets::Symmetric(10, 0)),
                CrossAlign(CrossAxisAlignment::Center),
                Background(selected ? theme.colors.secondary_container : Color::Transparent()),
                CornerRadius(10.0F),
                Frame{.height = 46.0F},
                Semantics{
                    .role = SemanticRole::Button,
                    .label = FolderLabel(folder),
                    .selected = selected,
                    .descendants = SemanticDescendantPolicy::Exclude,
                }
            )
    );
  }

  return Column{
      Column{std::move(folder_rows)}.With(Spacing(4), CrossAlign(CrossAxisAlignment::Stretch)),
      Spacer(),
      Divider(),
      Row{
          Row{
              Text("H").Style({Font::System(13.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_primary}),
          }
              .With(
                  MainAlign(MainAxisAlignment::Center),
                  CrossAlign(CrossAxisAlignment::Center),
                  Frame{.width = 32.0F, .height = 32.0F},
                  Background(theme.colors.primary),
                  CornerRadius(16.0F)
              ),
          Text("Huxer Listener")
              .Style({Font::System(13.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface})
              .With(Grow()),
          IconButton(dark ? app::images::sun : app::images::moon,
                     dark ? StringVariant(app::strings::theme_light) : StringVariant(app::strings::theme_dark))
              .OnClick(toggle_theme)
              .With(Tooltip(
                  dark ? StringVariant(app::strings::theme_light) : StringVariant(app::strings::theme_dark)
              )),
      }
          .With(Spacing(10), CrossAlign(CrossAxisAlignment::Center), Frame{.height = 54.0F}),
  }
      .With(
          Spacing(12),
          Background(theme.colors.background),
          Padding(EdgeInsets{14, 14, 14, 14}),
          Frame{.width = 224.0F}
      );
}

[[huxerui::composable]]
View StatusLine(const AppStatusContext& status, const InteractionContext& interaction) {
  const ThemeSpec& theme = UseTheme();
  const SyncStatus sync = status.sync.Get();
  const std::string label = UseString(SyncLabel(sync.phase));
  return Row{
      sync.phase == SyncPhase::Syncing
          ? View(ProgressCircle().With(Frame{.width = 16.0F, .height = 16.0F}))
          : View(Image(app::images::check).Tint(theme.colors.primary).With(Frame{.width = 16.0F, .height = 16.0F})),
      Text(label).Style({Font::System(12.0F), theme.colors.on_surface_variant}),
  }
      .With(
          Spacing(6),
          CrossAlign(CrossAxisAlignment::Center),
          Semantics{
              .label = StringVariant::Format(app::strings::sync_status_label, label),
              .value = interaction.live_announcement.Get(),
              .busy = sync.phase == SyncPhase::Syncing,
              .live_region = SemanticLiveRegion::Polite,
          }
      );
}

[[huxerui::composable]]
View SelectionToolbar() {
  const MailboxDataContext data = UseEnvironment<MailboxDataContext>();
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const InteractionContext interaction = UseEnvironment<InteractionContext>();
  const ToastHandle toast = UseToast();
  const ThemeSpec& theme = UseTheme();
  const std::size_t count = interaction.selected_ids.Get().size();
  const std::vector<MailThread> projected = ProjectThreads(data, navigation, interaction);
  const bool all_selected = !projected.empty() && std::ranges::all_of(projected, [interaction](const MailThread& thread) {
                              return ContainsId(interaction.selected_ids.Get(), thread.id);
                            });

  Views actions;
  actions.Add(ProvideEnvironment(
      MailCheckboxStyle(theme),
      Checkbox(all_selected)
          .OnChanged([interaction, projected](bool selected) {
            if (!selected) {
              interaction.selected_ids = std::vector<std::string>{};
              return;
            }
            std::vector<std::string> ids;
            ids.reserve(projected.size());
            for (const MailThread& thread : projected) {
              ids.push_back(thread.id);
            }
            interaction.selected_ids = std::move(ids);
          })
          .With(
              Semantics{
                  .label = all_selected ? StringVariant(app::strings::deselect_all)
                                        : StringVariant(app::strings::select_all),
              },
              Tooltip(
                  all_selected ? StringVariant(app::strings::deselect_all) : StringVariant(app::strings::select_all)
              )
          )
  ));
  actions.Add(
      Text::Format(app::strings::selection_count, count)
          .Style({Font::System(13.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface})
          .With(Grow())
  );
  actions.Add(IconButton(app::images::archive, app::strings::archive)
                  .OnClick([=] {
                    ApplyBulkMove(data, navigation, interaction, MailFolder::Archive, UndoKind::Archive);
                  })
                  .With(Enabled(count > 0), Tooltip(app::strings::archive)));
  actions.Add(IconButton(app::images::check, app::strings::mark_read)
                  .OnClick([=] {
                    ApplyBulkRead(data, interaction);
                    toast.Show(app::strings::mark_read);
                  })
                  .With(Enabled(count > 0), Tooltip(app::strings::mark_read)));
  actions.Add(IconButton(app::images::trash, app::strings::resource_delete)
                  .OnClick([=] {
                    ApplyBulkMove(data, navigation, interaction, MailFolder::Trash, UndoKind::Delete);
                  })
                  .With(Enabled(count > 0), Tooltip(app::strings::resource_delete)));
  actions.Add(IconButton(app::images::close, app::strings::select_done)
                  .OnClick([interaction] { ClearSelection(interaction); })
                  .With(Tooltip(app::strings::select_done)));
  return Row{std::move(actions)}.With(
      Spacing(4),
      CrossAlign(CrossAxisAlignment::Center),
      Padding(EdgeInsets::Symmetric(14, 8)),
      Background(theme.colors.surface_container_low)
  );
}

[[huxerui::composable]]
View MailRow(MailThread thread, MailNavigator navigator) {
  const MailboxDataContext data = UseEnvironment<MailboxDataContext>();
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const InteractionContext interaction = UseEnvironment<InteractionContext>();
  const AppStatusContext status = UseEnvironment<AppStatusContext>();
  const ThemeSpec& theme = UseTheme();
  const std::string reply_subject = UseString(app::strings::reply_subject, thread.subject);
  const bool selected = ContainsId(interaction.selected_ids.Get(), thread.id);
  const bool focused = navigation.focused_thread.Get() == std::optional<std::string>(thread.id);
  const std::size_t attachment_count = AttachmentCount(thread);
  const int relative_minutes = thread.relative_minutes + status.clock_revision.Get();
  const std::string time = RelativeTime(
      relative_minutes,
      UseString(app::strings::time_now),
      UseString(app::strings::time_minutes, relative_minutes),
      UseString(app::strings::time_hours, relative_minutes / 60),
      UseString(app::strings::time_days, relative_minutes / 1440)
  );
  const std::string semantic_label = UseString(
      app::strings::mail_row_semantics,
      thread.sender_name,
      thread.subject,
      time,
      UseString(thread.unread ? app::strings::state_unread : app::strings::state_read),
      UseString(thread.starred ? app::strings::state_starred : app::strings::state_not_starred),
      UseString(attachment_count > 0 ? app::strings::state_has_attachments : app::strings::state_no_attachments)
  );
  const float selected_progress = selected ? 1.0F : 0.0F;
  const float focused_progress = focused && !selected ? 1.0F : 0.0F;
  const Color hover_fill =
      status.theme.Get() == ThemeMode::Dark ? Color::Rgb(164, 145, 255, 0.10F)
                                            : Color::Rgb(116, 87, 245, 0.05F);
  const Color press_fill =
      status.theme.Get() == ThemeMode::Dark ? Color::Rgb(164, 145, 255, 0.17F)
                                            : Color::Rgb(116, 87, 245, 0.10F);

  Views badges;
  if (thread.failed) {
    badges.Add(Text(app::strings::failed_badge)
                   .Style({Font::System(11.0F).WithWeight(FontWeight::SemiBold), theme.colors.error}));
  }
  if (attachment_count > 0) {
    badges.Add(Image(app::images::attachment)
                   .Tint(theme.colors.on_surface_variant)
                   .With(Frame{.width = 15.0F, .height = 15.0F}));
  }
  if (thread.messages.size() > 1) {
    badges.Add(
        Text::Format(app::strings::thread_messages, thread.messages.size())
            .Style({Font::System(11.0F), theme.colors.on_surface_variant})
    );
  }

  const TextStyle sender_style{
      Font::System(14.5F).WithWeight(thread.unread ? FontWeight::SemiBold : FontWeight::Medium),
      theme.colors.on_surface,
  };
  const TextStyle subject_style{
      Font::System(14.0F).WithWeight(thread.unread ? FontWeight::SemiBold : FontWeight::Regular),
      theme.colors.on_surface,
  };
  View avatar =
      Row{Text(FirstGlyph(thread.sender_name))
              .Style({Font::System(13.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface_variant})}
          .With(
              Frame{.width = 38.0F, .height = 38.0F},
              MainAlign(MainAxisAlignment::Center),
              CrossAlign(CrossAxisAlignment::Center),
              Background(AvatarColor(thread.id, status.theme.Get() == ThemeMode::Dark)),
              CornerRadius(9.0F)
          );

  Views leading;
  if (interaction.selection_mode.Get()) {
    leading.Add(ProvideEnvironment(
        MailCheckboxStyle(theme),
        Checkbox(selected).OnChanged([interaction, id = thread.id](bool) { ToggleSelected(interaction, id); })
    ));
  }
  leading.Add(std::move(avatar));

  View content =
      Column{
          Row{
              thread.unread ? View(
                                  Row{}.With(
                                      Frame{.width = 6.0F, .height = 6.0F},
                                      Background(theme.colors.primary),
                                      CornerRadius(3.0F)
                                  )
                              )
                            : View(Row{}.With(Frame{.width = 0.0F, .height = 0.0F})),
              Text(thread.sender_name).Style(sender_style).With(Grow()),
              Text(time).Style({Font::System(11.0F), theme.colors.on_surface_variant}),
              thread.starred
                  ? View(
                        Image(app::images::star).Tint(theme.colors.primary).With(Frame{.width = 15.0F, .height = 15.0F})
                    )
                  : View(Spacer().With(Frame{.width = 0.0F, .height = 0.0F})),
          }
              .With(Spacing(8), CrossAlign(CrossAxisAlignment::Center)),
          Text(thread.subject.empty() ? UseString(app::strings::no_subject) : thread.subject).Style(subject_style),
          Text(thread.excerpt)
              .Style({Font::System(12.5F), theme.colors.on_surface_variant})
              .With(Frame{.max_height = 20.0F}, ClipChildren()),
          Row{std::move(badges)}.With(Spacing(8), CrossAlign(CrossAxisAlignment::Center)),
      }
          .With(Spacing(5), Grow());

  return Stack{
      Spacer().With(
          Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch),
          Background(theme.colors.surface_container_low),
          Opacity(AnimateTo(focused_progress, TweenSpec{theme.motion.fast, Easing::EaseOut}))
      ),
      Spacer().With(
          Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch),
          Background(theme.colors.secondary_container),
          Opacity(AnimateTo(selected_progress, TweenSpec{theme.motion.fast, Easing::EaseOut}))
      ),
      Spacer().With(
          Frame{.width = 3.0F},
          Align(HorizontalAlignment::Start, VerticalAlignment::Stretch),
          Background(theme.colors.primary),
          Opacity(AnimateTo(focused_progress, TweenSpec{theme.motion.fast, Easing::EaseOut}))
      ),
      Column{
          Row{
              Row{std::move(leading)}.With(Spacing(10), CrossAlign(CrossAxisAlignment::Center)),
              std::move(content),
          }
              .With(Spacing(12), Padding(EdgeInsets::Symmetric(14, 13)), CrossAlign(CrossAxisAlignment::Start)),
          Divider(),
      },
  }
      .OnClick([=] {
        if (interaction.selection_mode.Get()) {
          ToggleSelected(interaction, thread.id);
          return;
        }
        if (thread.folder == MailFolder::Drafts) {
          PrepareComposer(thread, false, navigation, interaction, navigator, reply_subject);
          return;
        }
        SetUnread(data, thread.id, false);
        NavigateTo({.kind = MailRouteKind::Reader, .value = thread.id}, navigation, navigator);
      })
      .With(
          Indication{
              .hover = IndicationLayer{.fill = hover_fill},
              .press = IndicationLayer{.fill = press_fill},
          },
          Background(theme.colors.surface),
          Semantics{
              .role = SemanticRole::ListItem,
              .label = semantic_label,
              .hint = StringVariant::Format(app::strings::open_thread, thread.subject),
              .state_description = thread.failed ? StringVariant(app::strings::state_delivery_failed) : StringVariant{},
              .selected = selected || focused,
              .collection_item = SemanticCollectionItem{},
          }
      );
}

[[huxerui::composable]]
View EmptyMailView(bool searching) {
  const ThemeSpec& theme = UseTheme();
  return Column{
      Text(searching ? app::strings::search_no_results_title : app::strings::mail_empty_title, TextRole::Title)
          .Style({Font::System(20.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
      Text(searching ? app::strings::search_no_results_body : app::strings::mail_empty_body)
          .Style({Font::System(14.0F), theme.colors.on_surface_variant}),
  }
      .With(
          Spacing(10),
          Padding(EdgeInsets::All(32)),
          MainAlign(MainAxisAlignment::Center),
          CrossAlign(CrossAxisAlignment::Center),
          Grow()
      );
}

[[huxerui::composable]]
View MailListPanel(ViewportClass viewport, TaskScope mailbox_scope) {
  const MailboxDataContext data = UseEnvironment<MailboxDataContext>();
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const InteractionContext interaction = UseEnvironment<InteractionContext>();
  const AppStatusContext status = UseEnvironment<AppStatusContext>();
  const ThemeSpec& theme = UseTheme();
  const MailNavigator navigator =
      viewport == ViewportClass::Compact ? MailNavigator{UseNavigation<MailRoute>()} : MailNavigator{};
  const ToastHandle toast = UseToast();
  const MenuHandle filter_menu = UseMenu();
  const MenuHandle action_menu = UseMenu();
  const std::shared_ptr<MockMailService> service = UseService<MockMailService>();
  const auto search_task = UseState(std::make_shared<TaskHandle>());
  const std::shared_ptr<TaskHandle> pending_search = search_task.Get();
  const MailThread notification = LocalizedSystemNotification(
      UseString(app::strings::new_system_sender),
      UseString(app::strings::new_system_email),
      UseString(app::strings::new_system_subject),
      UseString(app::strings::new_system_body),
      UseString(app::strings::you)
  );
  const std::string live_started = UseString(app::strings::sync_live_started);
  const std::string live_failed = UseString(app::strings::sync_live_failed);
  const std::string live_succeeded = UseString(app::strings::sync_live_succeeded);

  if (interaction.selection_mode.Get()) {
    return Column{
        SelectionToolbar(),
        VirtualList(
            ProjectThreads(data, navigation, interaction),
            [=](const MailThread& thread) {
              return MailRow(thread, navigator).Key(thread.id);
            }
        )
            .EstimatedItemExtent(106.0F)
            .CacheExtent(480.0F)
            .With(
                Grow(),
                ScrollBar(),
                Semantics{
                    .role = SemanticRole::List,
                    .label = app::strings::mail_list_semantics,
                }
            ),
    }
        .With(Background(theme.colors.surface), Grow());
  }

  TextField search(interaction.search);
  search = std::move(search)
               .Label(app::strings::search_label)
               .Placeholder(app::strings::search_placeholder)
               .LeadingIcon(app::images::search)
               .InputConfiguration({
                   .type = TextInputType::Text,
                   .action = TextInputAction::Search,
               })
               .OnChanged([=](const TextEditingValue& value) {
                 interaction.search = value;
                 pending_search->Cancel();
                 if (value.text.empty()) {
                   interaction.search_loading = false;
                   interaction.search_results = std::vector<std::string>{};
                   return;
                 }
                 interaction.search_loading = true;
                 const std::vector<MailThread> snapshot = SnapshotThreads(data.threads);
                 const std::string query = value.text;
                 *pending_search = mailbox_scope.Launch([=]() -> Task<void> {
                   std::vector<std::string> results = co_await service->Search(snapshot, query);
                   if (interaction.search.Get().text == query) {
                     interaction.search_results = std::move(results);
                     interaction.search_loading = false;
                   }
                 });
               });

  Views header_actions;
  if (viewport != ViewportClass::Compact) {
    header_actions.Add(
        Row{
            Image(app::images::compose).Tint(theme.colors.primary).With(Frame{.width = 16.0F, .height = 16.0F}),
            Text(app::strings::compose_action)
                .Style({Font::System(13.0F).WithWeight(FontWeight::SemiBold), theme.colors.primary}),
        }
            .OnClick([=] { BeginNewComposer(data, navigation, interaction, navigator); })
            .With(
                Spacing(7),
                Padding(EdgeInsets::Symmetric(12, 0)),
                MainAlign(MainAxisAlignment::Center),
                CrossAlign(CrossAxisAlignment::Center),
                Background(theme.colors.secondary_container),
                CornerRadius(9.0F),
                Frame{.width = 104.0F, .height = 40.0F},
                Semantics{
                    .role = SemanticRole::Button,
                    .label = app::strings::compose_action,
                    .descendants = SemanticDescendantPolicy::Exclude,
                }
            )
    );
  }
  header_actions.Add(
      IconButton(app::images::search, app::strings::search_label)
          .OnClick([=] {
            if (!interaction.search_open.Get()) {
              interaction.search_open = true;
              return;
            }
            pending_search->Cancel();
            interaction.search = TextEditingValue::FromText({});
            interaction.search_loading = false;
            interaction.search_results = std::vector<std::string>{};
            interaction.search_open = false;
          })
          .With(
              Tooltip(app::strings::search_label),
              Background(theme.colors.surface_container),
              CornerRadius(9.0F),
              Frame{.width = 40.0F, .height = 40.0F}
          )
  );
  const MailFilter current_filter = navigation.filter.Get();
  const StringVariant filter_tooltip = StringVariant::Format(
      app::strings::filter_current,
      UseString(FilterLabel(current_filter))
  );
  IconButtonStyle filter_button_style = IconButtonStyle::Default();
  filter_button_style.foreground =
      current_filter == MailFilter::All ? theme.colors.on_surface_variant : theme.colors.primary;
  filter_button_style.disabled_foreground = theme.colors.outline;
  filter_button_style.icon_size = 19.0F;
  filter_button_style.minimum_interactive_size = 40.0F;
  filter_button_style.state_layer_size = 34.0F;
  filter_button_style.corner_radius = 10.0F;
  ThemeDefinition filter_button_theme;
  filter_button_theme.Set(filter_button_style);
  header_actions.Add(
      Theme(
          std::move(filter_button_theme),
          IconButton(app::images::filter, filter_tooltip)
              .With(
                  filter_menu.Anchor(),
                  Tooltip(filter_tooltip),
                  Background(theme.colors.surface_container),
                  CornerRadius(9.0F),
                  Frame{.width = 40.0F, .height = 40.0F}
              )
              .OnClick([=] {
                filter_menu.Show({
                    MenuItem(app::strings::filter_all, [navigation] { navigation.filter = MailFilter::All; })
                        .Checked(current_filter == MailFilter::All),
                    MenuItem(app::strings::filter_unread, [navigation] { navigation.filter = MailFilter::Unread; })
                        .Checked(current_filter == MailFilter::Unread),
                    MenuItem(app::strings::filter_starred, [navigation] { navigation.filter = MailFilter::Starred; })
                        .Checked(current_filter == MailFilter::Starred),
                });
              })
      )
  );
  header_actions.Add(
      IconButton(app::images::more, app::strings::more)
          .With(
              action_menu.Anchor(),
              Tooltip(app::strings::more),
              Background(theme.colors.surface_container),
              CornerRadius(9.0F),
              Frame{.width = 40.0F, .height = 40.0F}
          )
          .OnClick([=] {
            action_menu.Show({
                MenuItem(app::strings::select, [interaction] { interaction.selection_mode = true; }),
                MenuItem(
                    status.sync.Get().retry_available ? StringVariant(app::strings::retry)
                                                      : StringVariant(app::strings::refresh),
                    [=] {
                      StartRefresh(
                          data,
                          status,
                          interaction,
                          mailbox_scope,
                          service,
                          toast,
                          notification,
                          live_started,
                          live_failed,
                          live_succeeded
                      );
                    }
                ),
            });
          })
  );

  Views search_row;
  search_row.Add(std::move(search).With(Grow()));
  if (!interaction.search.Get().text.empty()) {
    search_row.Add(IconButton(app::images::close, app::strings::search_clear)
                       .OnClick([=] {
                         pending_search->Cancel();
                         interaction.search = TextEditingValue::FromText({});
                         interaction.search_loading = false;
                         interaction.search_results = std::vector<std::string>{};
                       })
                       .With(Tooltip(app::strings::search_clear)));
  }

  const std::vector<MailThread> projected = ProjectThreads(data, navigation, interaction);
  View results =
      interaction.search_loading.Get() ? View(
                                             Column{
                                                 ProgressCircle(),
                                                 Text(app::strings::searching),
                                             }
                                                 .With(
                                                     Spacing(12),
                                                     MainAlign(MainAxisAlignment::Center),
                                                     CrossAlign(CrossAxisAlignment::Center),
                                                     Grow()
                                                 )
                                         )
      : projected.empty()
          ? EmptyMailView(!interaction.search.Get().text.empty())
          : View(VirtualList(
                     projected,
                     [=](const MailThread& thread) {
                       return MailRow(thread, navigator).Key(thread.id);
                     }
            )
                     .EstimatedItemExtent(106.0F)
                     .CacheExtent(480.0F)
                     .With(
                         Grow(),
                         ScrollBar(),
                         Semantics{
                             .role = SemanticRole::List,
                             .label = app::strings::mail_list_semantics,
                             .collection = SemanticCollection{
                                 .item_count = projected.size(),
                             },
                         }
                     ));

  View header = Row{
      Column{
          Text(FolderLabel(navigation.folder.Get()), TextRole::Title)
              .Style({
                  Font::System(viewport == ViewportClass::Compact ? 22.0F : 28.0F).WithWeight(FontWeight::SemiBold),
                  theme.colors.on_surface,
              }),
          navigation.folder.Get() == MailFolder::Inbox
              ? View(Text::Format(app::strings::folder_unread_count, InboxUnreadCount(data.threads))
                         .Style({Font::System(13.0F), theme.colors.on_surface_variant}))
              : StatusLine(status, interaction),
      }
          .With(Spacing(4), Grow()),
      Row{std::move(header_actions)}.With(Spacing(6), CrossAlign(CrossAxisAlignment::Center)),
  }.With(CrossAlign(CrossAxisAlignment::Center));

  const std::size_t selected_filter = navigation.filter.Get() == MailFilter::Unread ? 0U : 1U;
  TabsStyle filter_tabs_style = TabsStyle::Default();
  filter_tabs_style.label_style = TextStyle{Font::System(14.0F), theme.colors.on_surface_variant};
  filter_tabs_style.selected_label = theme.colors.primary;
  filter_tabs_style.disabled_label = theme.colors.outline;
  filter_tabs_style.indicator = theme.colors.primary;
  filter_tabs_style.indicator_sizing = TabIndicatorSizing::Content;
  filter_tabs_style.indicator_min_width = 28.0F;
  ThemeDefinition filter_tabs_theme;
  filter_tabs_theme.Set(std::move(filter_tabs_style));
  View filter_tabs = Theme(
      std::move(filter_tabs_theme),
      Tabs({FilterLabel(MailFilter::Unread), FilterLabel(MailFilter::All)}, selected_filter)
          .OnChanged([navigation](std::size_t index) {
            navigation.filter = index == 0U ? MailFilter::Unread : MailFilter::All;
          })
          .With(Frame{.width = 128.0F})
  );

  Views body;
  body.Add(std::move(header));
  if (interaction.search_open.Get()) {
    body.Add(Row{std::move(search_row)}.With(Spacing(6), CrossAlign(CrossAxisAlignment::Center)));
  }
  body.Add(std::move(filter_tabs));
  body.Add(Divider());
  body.Add(
      Text(app::strings::mail_recent)
          .Style({Font::System(12.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface_variant})
          .With(Padding(EdgeInsets{4, 0, 0, 2}))
  );
  body.Add(std::move(results));
  return Column{std::move(body)}.With(
      Spacing(12),
      Padding(EdgeInsets{20, 12, 0, 20}),
      Background(theme.colors.surface),
      Grow()
  );
}

[[huxerui::composable]]
View RoutePanel(bool compact) {
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const auto routes = navigation.path.Get().Routes();
  if (routes.empty()) {
    return ReaderView({}, compact).Key("reader-empty");
  }
  const MailRoute& route = routes.back();
  switch (route.kind) {
  case MailRouteKind::Reader:
    return ReaderView(route.value, compact).Key("reader-" + route.value);
  case MailRouteKind::Composer:
    return ComposerView(compact).Key("composer-" + route.value);
  }
  return ReaderView({}, compact).Key("reader-empty");
}

[[huxerui::composable]]
View CompactMailbox(TaskScope mailbox_scope) {
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const MailboxDataContext data = UseEnvironment<MailboxDataContext>();
  const InteractionContext interaction = UseEnvironment<InteractionContext>();
  const MailNavigator navigator{UseNavigation<MailRoute>()};
  const ViewportClass viewport = ViewportClass::Compact;
  std::vector<View> actions;
  actions.push_back(IconButton(app::images::compose, app::strings::compose)
                        .OnClick([=] { BeginNewComposer(data, navigation, interaction, navigator); })
                        .With(Tooltip(app::strings::compose)));
  View leading = IconButton(app::images::menu, app::strings::drawer_label).OnClick([navigation] {
    navigation.drawer_open = true;
  });
  View content =
      Column{
          TopAppBar(FolderLabel(navigation.folder.Get()), std::move(leading), std::move(actions)),
          MailListPanel(viewport, mailbox_scope),
      }
          .With(Grow());
  return DrawerLayout{
      std::move(content),
      StartDrawer{NavigationPanel(true, viewport)}
          .Open(navigation.drawer_open)
          .OnOpenChanged([navigation](bool open) { navigation.drawer_open = open; })
          .With(
              Semantics{
                  .role = SemanticRole::Navigation,
                  .label = app::strings::drawer_label,
              }
          ),
  };
}

[[huxerui::composable]]
View MailboxScene(ViewportClass viewport) {
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const InteractionContext interaction = UseEnvironment<InteractionContext>();
  const MailboxDataContext data = UseEnvironment<MailboxDataContext>();
  const AppStatusContext status = UseEnvironment<AppStatusContext>();
  const ThemeSpec& theme = UseTheme();
  const TaskScope mailbox_scope = UseTaskScope();
  const std::optional<UndoRecord> undo = interaction.undo.Get();
  Lifecycle(
      [mailbox_scope, undo_state = interaction.undo, undo] {
        TaskHandle timeout;
        if (undo) {
          timeout = mailbox_scope.Launch([undo_state, token = undo->token]() -> Task<void> {
            co_await Delay(5s);
            const std::optional<UndoRecord> current = undo_state.Get();
            if (current && current->token == token) {
              undo_state = std::nullopt;
            }
          });
        }
        return [timeout] { timeout.Cancel(); };
      },
      undo
  );

  View content;
  if (viewport == ViewportClass::Compact) {
    content = CompactMailbox(mailbox_scope);
  } else if (viewport == ViewportClass::Expanded) {
    content = Row{
        NavigationPanel(true, viewport),
        Row{
            MailListPanel(viewport, mailbox_scope).With(Frame{.width = 440.0F}),
            Divider(Axis::Vertical),
            RoutePanel(false).With(Grow()),
        }
            .With(
                Grow(),
                Background(theme.colors.surface),
                Border{theme.colors.outline, 1.0F},
                CornerRadius(16.0F),
                ClipChildren(),
                Shadow(Color::Rgb(35, 24, 82, status.theme.Get() == ThemeMode::Dark ? 0.22F : 0.07F),
                       {0, 5},
                       28,
                       0)
            ),
    }.With(Spacing(8), Padding(EdgeInsets{8, 14, 14, 0}));
  } else {
    const auto routes = navigation.path.Get().Routes();
    const bool focus_mode = !routes.empty() && routes.back().kind == MailRouteKind::Composer;
    content = focus_mode
                  ? View(
                        Row{
                            NavigationPanel(false, viewport),
                            RoutePanel(false)
                                .With(
                                    Grow(),
                                    Background(theme.colors.surface),
                                    Border{theme.colors.outline, 1.0F},
                                    CornerRadius(14.0F),
                                    ClipChildren()
                                ),
                        }
                            .With(Spacing(8), Padding(EdgeInsets{8, 12, 12, 0}))
                    )
                  : View(
                        Row{
                            NavigationPanel(false, viewport),
                            Row{
                                MailListPanel(viewport, mailbox_scope).With(Frame{.width = 372.0F}),
                                Divider(Axis::Vertical),
                                RoutePanel(false).With(Grow()),
                            }
                                .With(
                                    Grow(),
                                    Background(theme.colors.surface),
                                    Border{theme.colors.outline, 1.0F},
                                    CornerRadius(14.0F),
                                    ClipChildren()
                                ),
                        }
                            .With(Spacing(8), Padding(EdgeInsets{8, 12, 12, 0}))
                    );
  }

  Views layers;
  layers.Add(std::move(content));
  if (undo) {
    const StringResource message = undo->kind == UndoKind::Archive ? app::strings::archived : app::strings::deleted;
    const bool dark = status.theme.Get() == ThemeMode::Dark;
    const Color snackbar_background = dark ? theme.colors.surface_container_highest : theme.colors.inverse_surface;
    const Color snackbar_foreground = dark ? theme.colors.on_surface : theme.colors.inverse_on_surface;
    layers.Add(
        Column{
            Spacer(),
            Row{
                Spacer(),
                Row{
                    Text(message).Style({Font::System(14.0F).WithWeight(FontWeight::Medium), snackbar_foreground}),
                    Text(app::strings::undo)
                        .Style({Font::System(14.0F).WithWeight(FontWeight::SemiBold), snackbar_foreground})
                        .OnClick([=] { UndoLastMove(data, navigation, interaction); })
                        .With(
                            Padding(EdgeInsets::Symmetric(10, 6)),
                            CornerRadius(8),
                            Semantics{
                                .role = SemanticRole::Button,
                                .label = app::strings::undo,
                                .descendants = SemanticDescendantPolicy::Exclude,
                            }
                        ),
                }
                    .With(
                        Spacing(14),
                        CrossAlign(CrossAxisAlignment::Center),
                        Padding(EdgeInsets{10, 14, 10, 8}),
                        Background(snackbar_background),
                        CornerRadius(12),
                        Shadow(Color::Rgb(0, 0, 0, dark ? 0.24F : 0.16F), {0, 0}, 14, 0),
                        Frame{.max_width = 420.0F}
                    ),
                Spacer(),
            },
        }
            .With(
                Padding(EdgeInsets{0, 20, 20, 20}),
                MainAlign(MainAxisAlignment::End)
            )
    );
  }
  return Stack{std::move(layers)}.With(
      Background(theme.colors.background),
      SafeAreaPadding(),
      SystemBarsAppearance{
          .status_bar_background = theme.colors.surface,
          .navigation_bar_background = theme.colors.surface,
          .status_bar_content = status.theme.Get() == ThemeMode::Dark ? SystemBarContentBrightness::Light
                                                                      : SystemBarContentBrightness::Dark,
          .navigation_bar_content = status.theme.Get() == ThemeMode::Dark ? SystemBarContentBrightness::Light
                                                                          : SystemBarContentBrightness::Dark,
      }
  );
}

struct MailRouteCodec {
  std::string Encode(const NavigationPath<MailRoute>& path) const {
    if (path.Empty()) {
      return "#/";
    }
    const MailRoute& route = path.Routes().back();
    switch (route.kind) {
    case MailRouteKind::Reader:
      return "#/thread/" + route.value;
    case MailRouteKind::Composer:
      return "#/compose";
    }
    return "#/";
  }

  std::optional<NavigationPath<MailRoute>> Decode(std::string_view location) const {
    const std::size_t hash = location.find('#');
    const std::string_view fragment = hash == std::string_view::npos ? location : location.substr(hash);
    if (fragment.empty() || fragment == "#/" || fragment == "/") {
      return NavigationPath<MailRoute>{};
    }
    constexpr std::string_view thread_prefix = "#/thread/";
    if (fragment.starts_with(thread_prefix) && fragment.size() > thread_prefix.size()) {
      return NavigationPath<MailRoute>{MailRoute{
          .kind = MailRouteKind::Reader,
          .value = std::string(fragment.substr(thread_prefix.size())),
      }};
    }
    if (fragment == "#/compose") {
      return NavigationPath<MailRoute>{MailRoute{.kind = MailRouteKind::Composer}};
    }
    return std::nullopt;
  }
};

View ResolveCompactRoute(const MailRoute& route) {
  switch (route.kind) {
  case MailRouteKind::Reader:
    return ReaderView(route.value, true);
  case MailRouteKind::Composer:
    return ComposerView(true);
  }
  return ReaderView({}, true);
}

#if defined(__EMSCRIPTEN__)
View CompactNavigationRoot(State<NavigationPath<MailRoute>> path) {
  return web::BrowserNavigationStack<MailRoute>(
      [] { return MailboxScene(ViewportClass::Compact); },
      std::move(path),
      ResolveCompactRoute,
      MailRouteCodec{}
  );
}
#else
View CompactNavigationRoot(State<NavigationPath<MailRoute>> path) {
  return NavigationStack<MailRoute>(
      [] { return MailboxScene(ViewportClass::Compact); },
      std::move(path),
      ResolveCompactRoute
  );
}
#endif

[[huxerui::composable]]
View MailTitleBar() {
  const ThemeSpec& theme = UseTheme();
  return WindowTitleBar{
      Row{
          Image(app::images::huxer_mark).With(Frame{.width = 28.0F, .height = 28.0F}),
          Text(app::strings::app_name)
              .Style({Font::System(15.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
          Spacer(),
      }
          .With(
              Spacing(11.0F),
              Padding(EdgeInsets::Symmetric(20.0F, 0.0F)),
              CrossAlign(CrossAxisAlignment::Center),
              Grow()
          ),
  }.With(Background(theme.colors.background));
}

} // namespace

[[huxerui::composable]]
View MailboxRoot() {
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const ViewportClass viewport = UseViewportClass();
  const ThemeSpec& theme = UseTheme();
  View content;
  if (viewport == ViewportClass::Compact) {
    content = CompactNavigationRoot(navigation.path);
  } else {
    content = MailboxScene(viewport);
  }
  if constexpr (!kUsesDesktopWindowChrome) {
    return content;
  }
  return Column{
      MailTitleBar(),
      std::move(content).With(Grow()),
  }
      .With(
          CrossAlign(CrossAxisAlignment::Stretch),
          Background(theme.colors.background)
      );
}

} // namespace huxer_mail
