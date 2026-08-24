#include "mail_views.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <app_resources.h>
#include <huxerui/huxerui.h>

#include "mail_context.h"
#include "mail_theme.h"
#include "mock_mail_service.h"

namespace huxer_mail {

using namespace huxerui;

namespace {

void CloseReader(const NavigationContext& navigation) {
  navigation.path = NavigationPath<MailRoute>{};
  navigation.focused_thread = std::nullopt;
}

View TimeText(int minutes, Color color) {
  if (minutes < 1) {
    return Text(app::strings::time_now).Style({Font::System(12.0F), color});
  }
  if (minutes < 60) {
    return Text::Format(app::strings::time_minutes, minutes).Style({Font::System(12.0F), color});
  }
  if (minutes < 1440) {
    return Text::Format(app::strings::time_hours, minutes / 60).Style({Font::System(12.0F), color});
  }
  return Text::Format(app::strings::time_days, minutes / 1440).Style({Font::System(12.0F), color});
}

std::string SafeFileName(std::string name) {
  for (char& character : name) {
    const unsigned char value = static_cast<unsigned char>(character);
    if (value < 32 || std::string_view{"\\/:*?\"<>|"}.find(character) != std::string_view::npos) {
      character = '_';
    }
  }
  return name.empty() ? "attachment.bin" : name;
}

std::string ExtensionOf(std::string_view name) {
  const std::size_t dot = name.find_last_of('.');
  if (dot == std::string_view::npos || dot + 1 == name.size()) {
    return {};
  }
  std::string extension(name.substr(dot + 1));
  std::ranges::transform(extension, extension.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });
  return extension;
}

void PrepareReply(
    const MailThread& thread,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    std::string subject
) {
  interaction.composer = ComposerDraft{
      .id = "reply-" + thread.id + "-" + std::to_string(thread.messages.size() + 1),
      .thread_id = thread.id,
      .recipient = thread.sender_email,
      .subject = std::move(subject),
      .body = "\n\n",
      .reply = true,
  };
  interaction.composer_validation_attempted = false;
  interaction.attachment_errors = std::vector<std::string>{};
  navigation.path = NavigationPath<MailRoute>{MailRoute{.kind = MailRouteKind::Composer, .value = thread.id}};
}

void PrepareOutboxEdit(
    const MailThread& thread, const NavigationContext& navigation, const InteractionContext& interaction
) {
  const MailMessage& message = thread.messages.back();
  interaction.composer = ComposerDraft{
      .id = thread.id,
      .recipient = message.recipient_email,
      .subject = thread.subject,
      .body = message.body,
      .attachments = message.attachments,
  };
  interaction.composer_validation_attempted = false;
  interaction.attachment_errors = std::vector<std::string>{};
  navigation.path = NavigationPath<MailRoute>{MailRoute{.kind = MailRouteKind::Composer, .value = thread.id}};
}

void ExportAttachment(
    MailAttachment attachment,
    RawAsset mock_content,
    const TaskScope& tasks,
    const std::shared_ptr<FileSystem>& files,
    const std::shared_ptr<FilePicker>& picker,
    const ToastHandle& toast,
    std::string filter_name
) {
  if (!picker->CanSaveFiles()) {
    toast.Show(app::strings::attachment_save_unavailable);
    return;
  }
  const File temporary(files->Directories().temporary_directory, SafeFileName(attachment.name));
  if (attachment.origin == AttachmentOrigin::Mock) {
    if (!mock_content.HasValue() || !temporary.WriteBytes(mock_content.Bytes())) {
      toast.Show(app::strings::attachment_export_failed);
      return;
    }
    (void)tasks.Launch([=]() -> Task<void> {
      const bool saved = co_await picker->SaveFileAsync(
          temporary,
          {
              .suggested_name = attachment.name,
              .filter = {
                  .name = filter_name,
                  .extensions = {ExtensionOf(attachment.name)},
                  .content_types = {attachment.content_type},
              },
          }
      );
      toast.Show(
          saved ? StringVariant(app::strings::attachment_exported)
                : StringVariant(app::strings::attachment_export_canceled)
      );
    });
    return;
  }
  if (!attachment.reference) {
    toast.Show(app::strings::attachment_export_failed);
    return;
  }
  (void)tasks.Launch([=]() -> Task<void> {
    if (!co_await attachment.reference->ImportToAsync(temporary, true)) {
      toast.Show(app::strings::attachment_export_failed);
      co_return;
    }
    const bool saved = co_await picker->SaveFileAsync(
        temporary,
        {
            .suggested_name = attachment.name,
            .filter = {
                .name = filter_name,
                .extensions = {ExtensionOf(attachment.name)},
                .content_types = {attachment.content_type},
            },
        }
    );
    toast.Show(
        saved ? StringVariant(app::strings::attachment_exported)
              : StringVariant(app::strings::attachment_export_canceled)
    );
  });
}

void RetryOutbox(
    const MailboxDataContext& data,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    const AppTaskContext& tasks,
    const std::shared_ptr<MockMailService>& service,
    const ToastHandle& toast,
    std::string thread_id,
    std::string live_retrying,
    std::string live_success
) {
  if (interaction.sending.Get()) {
    return;
  }
  interaction.sending = true;
  interaction.live_announcement = std::move(live_retrying);
  (void)tasks.scope.Launch([=]() -> Task<void> {
    const bool succeeded = co_await service->RetryOutbox();
    interaction.sending = false;
    if (!succeeded) {
      toast.Show(app::strings::send_failed);
      co_return;
    }
    CompleteOutboxRetry(data, thread_id);
    interaction.live_announcement = live_success;
    navigation.folder = MailFolder::Sent;
    CloseReader(navigation);
    toast.Show(app::strings::outbox_retry_success);
  });
}

} // namespace

[[huxerui::composable]]
View ReaderView(std::string thread_id, bool compact) {
  const MailboxDataContext data = UseEnvironment<MailboxDataContext>();
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const InteractionContext interaction = UseEnvironment<InteractionContext>();
  const AppStatusContext status = UseEnvironment<AppStatusContext>();
  const AppTaskContext app_tasks = UseEnvironment<AppTaskContext>();
  const ThemeSpec& theme = UseTheme();
  const ToastHandle toast = UseToast();
  const DialogHandle dialog = UseDialog();
  const MenuHandle menu = UseMenu();
  const TaskScope reader_tasks = UseTaskScope();
  const std::shared_ptr<FileSystem> files = UseService<FileSystem>();
  const std::shared_ptr<FilePicker> picker = UseService<FilePicker>();
  const std::shared_ptr<MockMailService> service = UseService<MockMailService>();
  const auto entered = UseState(false);
  const std::optional<MailThread> thread = FindThread(data.threads, thread_id);
  const std::string you = UseString(app::strings::you);
  const std::string filter_name = UseString(app::strings::attachment_picker_name);
  const std::string live_retrying = UseString(app::strings::outbox_live_retrying);
  const std::string live_success = UseString(app::strings::outbox_live_success);
  const std::string retry_label = UseString(app::strings::retry);
  const std::string retrying_label = UseString(app::strings::outbox_retrying);
  const std::string no_subject = UseString(app::strings::no_subject);
  const std::string reply_subject = UseString(app::strings::reply_subject, thread ? thread->subject : std::string{});
  const RawAsset mock_document = UseRawResource(app::raw::offline_attachment_txt);
  const RawAsset mock_preview = UseRawResource(app::raw::workspace_preview_svg);
  Lifecycle([entered] { entered = true; }, thread_id);

  if (!thread) {
    return Column{
        Text(app::strings::reader_empty_title, TextRole::Title)
            .Style({Font::System(24.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
        Text(app::strings::reader_empty_body).Style({Font::System(15.0F), theme.colors.on_surface_variant}),
    }
        .With(
            Spacing(12),
            Padding(EdgeInsets::All(40)),
            MainAlign(MainAxisAlignment::Center),
            CrossAlign(CrossAxisAlignment::Center),
            Background(theme.colors.surface),
            Grow()
        );
  }

  const bool in_trash = thread->folder == MailFolder::Trash;
  const bool in_outbox = thread->folder == MailFolder::Outbox;
  const bool retrying = in_outbox && interaction.sending.Get();

  const auto archive = [=] {
    ApplyMoveWithUndo(data, navigation, interaction, thread->id, MailFolder::Archive, UndoKind::Archive);
  };
  const auto remove = [=] {
    if (in_trash) {
      dialog.Show(
          app::strings::permanent_delete_title,
          app::strings::permanent_delete_message,
          app::strings::permanently_delete,
          app::strings::cancel,
          [=] {
            (void)EraseThread(data.threads, thread->id);
            CloseReader(navigation);
            toast.Show(app::strings::permanent_deleted);
          },
          {},
          {.dismiss_on_outside_press = false}
      );
      return;
    }
    ApplyMoveWithUndo(data, navigation, interaction, thread->id, MailFolder::Trash, UndoKind::Delete);
  };

  std::optional<View> leading;
  if (compact) {
    leading = IconButton(app::images::back, app::strings::back).OnClick([navigation] { CloseReader(navigation); });
  }

  std::vector<View> actions;
  actions.push_back(IconButton(app::images::reply, app::strings::reply)
                        .OnClick([=] { PrepareReply(*thread, navigation, interaction, reply_subject); })
                        .With(Tooltip(app::strings::reply)));
  if (!compact) {
    actions.push_back(
        IconButton(app::images::star, thread->starred ? app::strings::unstar : app::strings::star)
            .OnClick([=] { ToggleStar(data, thread->id); })
            .With(Tooltip(thread->starred ? StringVariant(app::strings::unstar) : StringVariant(app::strings::star)))
    );
    actions.push_back(IconButton(app::images::archive, app::strings::archive)
                          .OnClick(archive)
                          .With(Tooltip(app::strings::archive), Enabled(!in_trash)));
    actions.push_back(
        IconButton(app::images::trash, in_trash ? app::strings::permanently_delete : app::strings::resource_delete)
            .OnClick(remove)
            .With(Tooltip(
                in_trash ? StringVariant(app::strings::permanently_delete)
                         : StringVariant(app::strings::resource_delete)
            ))
    );
  }
  actions.push_back(
      IconButton(app::images::more, app::strings::more)
          .With(menu.Anchor(), Tooltip(app::strings::reader_actions))
          .OnClick([=] {
            menu.Show({
                MenuItem(
                    app::images::star,
                    thread->starred ? StringVariant(app::strings::unstar) : StringVariant(app::strings::star),
                    [=] { ToggleStar(data, thread->id); }
                ).Checked(thread->starred),
                MenuItem(
                    thread->unread ? StringVariant(app::strings::mark_read) : StringVariant(app::strings::mark_unread),
                    [=] { SetUnread(data, thread->id, !thread->unread); }
                ),
                MenuSection{},
                MenuItem(app::images::archive, app::strings::archive, archive).Enabled(!in_trash),
                MenuItem(
                    app::images::trash,
                    in_trash ? StringVariant(app::strings::permanently_delete)
                             : StringVariant(app::strings::resource_delete),
                    remove
                ).IconTint(theme.colors.error),
            });
          })
  );

  Views messages;
  for (std::size_t index = 0; index < thread->messages.size(); ++index) {
    const MailMessage& message = thread->messages[index];
    const bool latest = index + 1 == thread->messages.size();
    const bool expanded = latest || ContainsId(interaction.expanded_messages.Get(), message.id);
    const std::string sender = message.direction == MessageDirection::Outgoing ? you : message.sender_name;
    const std::string recipient = message.direction == MessageDirection::Incoming ? you : message.recipient_name;
    Views message_content;
    message_content.Add(
        Row{
            Column{
                Text(sender).Style({Font::System(14.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
                Text::Format(app::strings::message_to, recipient)
                    .Style({Font::System(12.0F), theme.colors.on_surface_variant}),
            }
                .With(Spacing(2), Grow()),
            TimeText(message.relative_minutes + status.clock_revision.Get(), theme.colors.on_surface_variant),
        }
            .With(CrossAlign(CrossAxisAlignment::Start))
    );
    if (expanded) {
      message_content.Add(SelectionArea(Text(message.body)
                                            .Style({Font::System(16.0F), theme.colors.on_surface})
                                            .With(Padding(EdgeInsets::Symmetric(0, 10)))));
      if (index > 0) {
        message_content.Add(
            Column{
                Text(app::strings::message_quote)
                    .Style({Font::System(12.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface_variant}),
                Text(thread->messages[index - 1].body)
                    .Style({Font::System(13.0F), theme.colors.on_surface_variant})
                    .With(Frame{.max_height = 76.0F}, ClipChildren()),
            }
                .With(
                    Spacing(6),
                    Padding(EdgeInsets{12, 10, 12, 14}),
                    Background(theme.colors.surface_container_low),
                    CornerRadius(theme.shapes.small)
                )
        );
      }
      for (const MailAttachment& attachment : message.attachments) {
        const bool image_attachment = attachment.content_type.starts_with("image/");
        if (image_attachment) {
          message_content.Add(Image(app::images::workspace_preview)
                                  .Fit(ImageFit::Contain)
                                  .With(
                                      Frame{.height = 220.0F, .max_width = 420.0F},
                                      Background(theme.colors.surface_container_low),
                                      CornerRadius(theme.shapes.medium)
                                  ));
        }
        message_content.Add(
            Row{
                Image(app::images::attachment).Tint(theme.colors.primary).With(Frame{.width = 17.0F, .height = 17.0F}),
                Column{
                    Text(attachment.name)
                        .Style({Font::System(13.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface}),
                    attachment.size >= 1024 * 1024
                        ? View(
                              Text::Format(app::strings::attachment_size_mb, attachment.size / (1024 * 1024))
                                  .Style({Font::System(12.0F), theme.colors.on_surface_variant})
                          )
                        : View(
                              Text::Format(
                                  app::strings::attachment_size_kb,
                                  std::max<std::uint64_t>(1, attachment.size / 1024)
                              )
                                  .Style({Font::System(12.0F), theme.colors.on_surface_variant})
                          ),
                }
                    .With(Spacing(2), Grow()),
                IconButton(app::images::download, app::strings::export_attachment)
                    .OnClick([=] {
                      ExportAttachment(
                          attachment,
                          image_attachment ? mock_preview : mock_document,
                          reader_tasks,
                          files,
                          picker,
                          toast,
                          filter_name
                      );
                    })
                    .With(Enabled(picker->CanSaveFiles()), Tooltip(app::strings::export_attachment)),
            }
                .With(
                    Spacing(10),
                    Padding(EdgeInsets::Symmetric(12, 8)),
                    Background(theme.colors.surface_container_low),
                    CornerRadius(theme.shapes.small),
                    CrossAlign(CrossAxisAlignment::Center)
                )
        );
      }
    } else {
      message_content.Add(Text(message.body)
                              .Style({Font::System(13.0F), theme.colors.on_surface_variant})
                              .With(Frame{.max_height = 42.0F}, ClipChildren()));
    }
    if (!latest) {
      const StringResource expansion_label =
          expanded ? app::strings::collapse_message : app::strings::expand_message;
      const ImageResource expansion_icon = expanded ? app::images::chevron_up : app::images::chevron_down;
      message_content.Add(
          Row{
              Text(expansion_label)
                  .Style({Font::System(13.0F).WithWeight(FontWeight::SemiBold), theme.colors.primary}),
              Image(expansion_icon).Tint(theme.colors.primary).With(
                  Frame{.width = 16.0F, .height = 16.0F},
                  Semantics{.hidden = true}
              ),
          }
              .OnClick([interaction, id = message.id, expanded] {
                interaction.expanded_messages.Update([&id, expanded](std::vector<std::string>& values) {
                  if (expanded) {
                    std::erase(values, id);
                  } else if (!ContainsId(values, id)) {
                    values.push_back(id);
                  }
                });
              })
              .With(
                  Spacing(6),
                  CrossAlign(CrossAxisAlignment::Center),
                  Padding(EdgeInsets::Symmetric(10, 7)),
                  CornerRadius(8),
                  Semantics{
                      .role = SemanticRole::Button,
                      .label = expansion_label,
                      .expanded = expanded,
                      .descendants = SemanticDescendantPolicy::Exclude,
                  }
              )
      );
    }
    messages.Add(
        Column{
            Text(latest ? app::strings::message_latest : app::strings::message_history, TextRole::Label)
                .With(Foreground(theme.colors.primary)),
            Column{std::move(message_content)}.With(Spacing(10), CrossAlign(CrossAxisAlignment::Stretch)),
        }
            .Key(message.id)
            .With(
                Spacing(10),
                Padding(EdgeInsets::All(latest ? 22.0F : 18.0F)),
                Background(latest ? theme.colors.surface : theme.colors.surface_container_low),
                CornerRadius(theme.shapes.medium),
                Semantics{
                    .label = StringVariant::Format(app::strings::message_semantics, sender),
                    .expanded = expanded,
                }
            )
    );
  }

  Views status_block;
  if (in_outbox) {
    status_block.Add(
        Column{
            Row{
                Image(app::images::error).Tint(theme.colors.error).With(Frame{.width = 20.0F, .height = 20.0F}),
                Text(app::strings::outbox_title, TextRole::Title).With(Grow()),
            }
                .With(Spacing(10), CrossAlign(CrossAxisAlignment::Center)),
            Text(app::strings::outbox_failure_reason).With(Foreground(theme.colors.on_surface_variant)),
            Row{
                Spacer(),
                Row{
                    Text(app::strings::edit)
                        .Style({Font::System(14.0F).WithWeight(FontWeight::SemiBold), theme.colors.primary}),
                }
                    .OnClick([=] { PrepareOutboxEdit(*thread, navigation, interaction); })
                    .With(
                        MainAlign(MainAxisAlignment::Center),
                        CrossAlign(CrossAxisAlignment::Center),
                        Border{theme.colors.outline, 1.0F},
                        CornerRadius(10),
                        Frame{.width = 68.0F, .height = 40.0F},
                        Enabled(!retrying),
                        Semantics{
                            .role = SemanticRole::Button,
                            .label = app::strings::edit,
                            .descendants = SemanticDescendantPolicy::Exclude,
                        }
                    ),
                Button(retrying ? retrying_label : retry_label)
                    .OnClick([=] {
                      RetryOutbox(
                          data,
                          navigation,
                          interaction,
                          app_tasks,
                          service,
                          toast,
                          thread->id,
                          live_retrying,
                          live_success
                      );
                    })
                    .With(Enabled(!retrying), Frame{.width = 68.0F, .height = 40.0F}),
            }
                .With(Spacing(8), CrossAlign(CrossAxisAlignment::Center)),
        }
            .With(
                Spacing(10),
                Padding(EdgeInsets::All(16)),
                Background(theme.colors.surface_container_low),
                CornerRadius(theme.shapes.medium),
                Semantics{
                    .label = app::strings::outbox_failure_reason,
                    .busy = retrying,
                    .live_region = SemanticLiveRegion::Polite,
                }
            )
    );
  }

  return Column{
      TopAppBar(StringVariant(thread->sender_name), std::move(leading), std::move(actions)),
      ScrollView{
          Column{
              Text(thread->subject.empty() ? no_subject : thread->subject, TextRole::Title)
                  .Style(
                      {Font::System(26.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}
                  ),
              Row{
                  Text::Format(app::strings::thread_participants, thread->sender_name)
                      .Style({Font::System(14.0F), theme.colors.on_surface_variant})
                      .With(Grow()),
                  TimeText(thread->relative_minutes + status.clock_revision.Get(), theme.colors.on_surface_variant),
              }
                  .With(CrossAlign(CrossAxisAlignment::Center)),
              Column{std::move(status_block)}.With(Spacing(12)),
              Divider(),
              Column{std::move(messages)}.With(Spacing(12), CrossAlign(CrossAxisAlignment::Stretch)),
              Row{
                  Spacer(),
                  Row{
                      Image(app::images::reply).Tint(theme.colors.primary).With(Frame{.width = 17.0F, .height = 17.0F}),
                      Text(app::strings::reply)
                          .Style({Font::System(13.0F).WithWeight(FontWeight::SemiBold), theme.colors.primary}),
                  }
                      .OnClick([=] { PrepareReply(*thread, navigation, interaction, reply_subject); })
                      .With(
                          Spacing(6),
                          CrossAlign(CrossAxisAlignment::Center),
                          Padding(EdgeInsets::Symmetric(10, 7)),
                          CornerRadius(8),
                          Semantics{
                              .role = SemanticRole::Button,
                              .label = app::strings::reply,
                              .descendants = SemanticDescendantPolicy::Exclude,
                          }
                      ),
              },
          }
              .With(
                  Frame{.max_width = 840.0F},
                  Spacing(20),
                  Padding(compact ? EdgeInsets::All(20) : EdgeInsets::All(32)),
                  Background(theme.colors.surface),
                  CornerRadius(compact ? 0.0F : theme.shapes.large),
                  Shadow(Color::Rgb(0, 0, 0, compact ? 0.0F : 0.055F), {0, 0}, compact ? 0.0F : 36.0F, 0),
                  Align(HorizontalAlignment::Center, VerticalAlignment::Start),
                  CrossAlign(CrossAxisAlignment::Stretch),
                  Semantics{
                      .label = StringVariant::Format(app::strings::reader_thread_semantics, thread->subject),
                  }
              ),
      }
          .With(
              ScrollBar(),
              Padding(compact ? EdgeInsets{} : EdgeInsets::All(26)),
              Background(theme.colors.background),
              Grow()
          ),
  }
      .With(
          Background(theme.colors.background),
          Grow(),
          Transition{AnimateTo(entered.Get() ? 1.0F : 0.0F, TweenSpec{theme.motion.fast, Easing::EaseOut})}
              .Opacity(0.0F, 1.0F)
              .Offset({0.0F, theme.motion.reduced_motion ? 0.0F : 8.0F}, {})
      );
}

} // namespace huxer_mail
