#include "mail_views.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <app_resources.h>
#include <huxerui/huxerui.h>

#include "mail_context.h"
#include "mock_mail_service.h"

namespace huxer_mail {

using namespace huxerui;

namespace {

void CloseComposer(const NavigationContext& navigation) {
  navigation.path = NavigationPath<MailRoute>{};
  navigation.focused_thread = std::nullopt;
}

ValidationResult ValidateRecipient(std::string_view recipient) {
  return Validate(recipient, Required(app::strings::recipient_required), EmailAddress(app::strings::recipient_invalid));
}

StringVariant AttachmentSize(std::uint64_t size) {
  constexpr std::uint64_t bytes_per_megabyte = 1024 * 1024;
  if (size >= bytes_per_megabyte) {
    return StringVariant::Format(app::strings::attachment_size_mb, size / bytes_per_megabyte);
  }
  return StringVariant::Format(app::strings::attachment_size_kb, std::max<std::uint64_t>(1, size / 1024));
}

void StartSend(
    const MailboxDataContext& data,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    const AppTaskContext& tasks,
    const std::shared_ptr<MockMailService>& service,
    const ToastHandle& toast,
    std::string live_sending,
    std::string live_sent,
    std::string live_failed
) {
  if (interaction.sending.Get()) {
    return;
  }
  const ComposerDraft draft = interaction.composer.Get();
  interaction.sending = true;
  interaction.live_announcement = std::move(live_sending);
  (void)tasks.scope.Launch([=]() -> Task<void> {
    const bool sent = co_await service->Send();
    interaction.sending = false;
    if (!sent) {
      interaction.live_announcement = live_failed;
      toast.Show(app::strings::send_failed);
      co_return;
    }
    CompleteSend(data, interaction, draft);
    interaction.live_announcement = live_sent;
    CloseComposer(navigation);
    toast.Show(app::strings::message_sent);
  });
}

} // namespace

[[huxerui::composable]]
View ComposerView(bool compact) {
  const MailboxDataContext data = UseEnvironment<MailboxDataContext>();
  const InteractionContext interaction = UseEnvironment<InteractionContext>();
  const NavigationContext navigation = UseEnvironment<NavigationContext>();
  const AppTaskContext app_tasks = UseEnvironment<AppTaskContext>();
  const ThemeSpec& theme = UseTheme();
  const auto entered = UseState(false);
  Lifecycle([entered] { entered = true; });
  const ToastHandle toast = UseToast();
  const DialogHandle dialog = UseDialog();
  const TaskScope attachment_tasks = UseTaskScope();
  const std::shared_ptr<MockMailService> service = UseService<MockMailService>();
  const std::shared_ptr<FilePicker> picker = UseService<FilePicker>();
  const ComposerDraft draft = interaction.composer.Get();
  const bool sending = interaction.sending.Get();
  const bool validation_attempted = interaction.composer_validation_attempted.Get();
  const ValidationResult recipient_validation =
      validation_attempted ? ValidateRecipient(draft.recipient) : ValidationResult::None();
  const std::string picker_name = UseString(app::strings::attachment_picker_name);
  const std::string live_sending = UseString(app::strings::composer_live_sending);
  const std::string live_sent = UseString(app::strings::composer_live_sent);
  const std::string live_failed = UseString(app::strings::send_failed);
  const std::string send_label = UseString(app::strings::send);
  const std::string sending_label = UseString(app::strings::sending);

  std::optional<View> leading = IconButton(app::images::close, app::strings::close).OnClick([=] {
    const ComposerDraft current = interaction.composer.Get();
    if (current.Empty()) {
      CloseComposer(navigation);
      return;
    }
    dialog.Show(
        app::strings::discard_title,
        app::strings::discard_message,
        app::strings::save_draft,
        app::strings::discard,
        [=] {
          SaveComposerDraft(data, interaction);
          interaction.composer = ComposerDraft{};
          CloseComposer(navigation);
          toast.Show(app::strings::draft_saved);
        },
        [=] {
          interaction.composer = ComposerDraft{};
          CloseComposer(navigation);
          toast.Show(app::strings::draft_discarded);
        },
        {.dismiss_on_outside_press = false}
    );
  });

  Views attachments;
  if (draft.attachments.empty()) {
    attachments.Add(
        Text(app::strings::composer_no_attachments).Style({Font::System(13.0F), theme.colors.on_surface_variant})
    );
  }
  for (const MailAttachment& attachment : draft.attachments) {
    const StringVariant size = AttachmentSize(attachment.size);
    const bool megabytes = attachment.size >= 1024 * 1024;
    const std::uint64_t semantic_size =
        megabytes ? attachment.size / (1024 * 1024) : std::max<std::uint64_t>(1, attachment.size / 1024);
    View size_text = megabytes ? View(
                                     Text::Format(app::strings::attachment_size_mb, semantic_size)
                                         .Style({Font::System(12.0F), theme.colors.on_surface_variant})
                                 )
                               : View(
                                     Text::Format(app::strings::attachment_size_kb, semantic_size)
                                         .Style({Font::System(12.0F), theme.colors.on_surface_variant})
                                 );
    attachments.Add(
        Row{
            Image(app::images::attachment).Tint(theme.colors.primary).With(Frame{.width = 18.0F, .height = 18.0F}),
            Column{
                Text(attachment.name)
                    .Style({Font::System(13.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface}),
                std::move(size_text),
            }
                .With(Spacing(2), Grow()),
            IconButton(app::images::trash, app::strings::remove_attachment)
                .OnClick([interaction, id = attachment.id] {
                  interaction.composer.Update([&id](ComposerDraft& value) {
                    std::erase_if(value.attachments, [&id](const MailAttachment& item) { return item.id == id; });
                  });
                })
                .With(Tooltip(StringVariant::Format(app::strings::attachment_remove_hint, attachment.name))),
        }
            .With(
                Spacing(10),
                Padding(EdgeInsets::Symmetric(12, 8)),
                Background(theme.colors.surface_container_low),
                CornerRadius(theme.shapes.small),
                CrossAlign(CrossAxisAlignment::Center),
                Semantics{
                    .label = megabytes ? StringVariant::Format(
                                             app::strings::attachment_semantics_mb,
                                             attachment.name,
                                             semantic_size
                                         )
                                       : StringVariant::Format(
                                             app::strings::attachment_semantics_kb,
                                             attachment.name,
                                             semantic_size
                                         ),
                }
            )
    );
  }
  for (const std::string& name : interaction.attachment_errors.Get()) {
    attachments.Add(
        Row{
            Image(app::images::error).Tint(theme.colors.error).With(Frame{.width = 16.0F, .height = 16.0F}),
            Column{
                Text(name).Style({Font::System(12.0F).WithWeight(FontWeight::Medium), theme.colors.error}),
                Text(app::strings::attachment_read_failed).Style({Font::System(12.0F), theme.colors.error}),
            },
        }
            .With(Spacing(8), CrossAlign(CrossAxisAlignment::Center))
    );
  }

  TextField recipient(TextEditingValue::FromText(draft.recipient));
  recipient =
      std::move(recipient)
          .Label(app::strings::recipient_label)
          .Placeholder(app::strings::recipient_placeholder)
          .Validation(recipient_validation)
          .InputConfiguration({
              .type = TextInputType::Email,
              .action = TextInputAction::Next,
          })
          .OnChanged([interaction](const TextEditingValue& value) {
            interaction.composer.Update([&value](ComposerDraft& draft_value) { draft_value.recipient = value.text; });
          });

  TextField subject(TextEditingValue::FromText(draft.subject));
  subject =
      std::move(subject)
          .Label(app::strings::subject_label)
          .Placeholder(app::strings::subject_placeholder)
          .OnChanged([interaction](const TextEditingValue& value) {
            interaction.composer.Update([&value](ComposerDraft& draft_value) { draft_value.subject = value.text; });
          });

  TextField body(TextEditingValue::FromText(draft.body));
  body = std::move(body)
             .Label(app::strings::body_label)
             .Placeholder(app::strings::body_placeholder)
             .LineLimits(TextFieldLineLimits::MultiLine(compact ? 8 : 12, compact ? 14 : 22))
             .OnChanged([interaction](const TextEditingValue& value) {
               interaction.composer.Update([&value](ComposerDraft& draft_value) { draft_value.body = value.text; });
             });

  Views attachment_controls;
  attachment_controls.Add(
      Row{
          Image(app::images::attachment)
              .Tint(!sending && picker->CanOpenFiles() ? theme.colors.primary : theme.colors.outline)
              .With(Frame{.width = 17.0F, .height = 17.0F}),
          Text(app::strings::add_attachment)
              .Style({
                  Font::System(13.0F).WithWeight(FontWeight::SemiBold),
                  !sending && picker->CanOpenFiles() ? theme.colors.primary : theme.colors.outline,
              }),
      }
          .OnClick([=] {
            (void)attachment_tasks.Launch([=]() -> Task<void> {
              std::vector<FileReference> selected = co_await picker->OpenFilesAsync({
                  .name = picker_name,
                  .extensions = {"png", "jpg", "jpeg", "pdf", "txt"},
                  .content_types = {"image/*", "application/pdf", "text/plain"},
              });
              for (FileReference& reference : selected) {
                FileResult<std::vector<std::byte>> content = co_await reference.ReadBytesAsync();
                if (!content.Succeeded()) {
                  interaction.attachment_errors.Update([name = reference.Name()](std::vector<std::string>& errors) {
                    errors.push_back(name);
                  });
                  continue;
                }
                const std::uint64_t size = reference.Size().value_or(content.Value().size());
                MailAttachment attachment{
                    .id = "picked-" + std::to_string(interaction.composer.Get().attachments.size() + 1) + "-" +
                          reference.Name(),
                    .name = reference.Name(),
                    .size = size,
                    .content_type = reference.ContentType().value_or("application/octet-stream"),
                    .origin = AttachmentOrigin::Picked,
                    .reference = reference,
                };
                interaction.composer.Update([&attachment](ComposerDraft& value) {
                  value.attachments.push_back(std::move(attachment));
                });
                toast.Show(app::strings::attachment_added);
              }
            });
          })
          .With(
              Spacing(6),
              CrossAlign(CrossAxisAlignment::Center),
              Padding(EdgeInsets::Symmetric(10, 7)),
              CornerRadius(8),
              Enabled(!sending && picker->CanOpenFiles()),
              Semantics{
                  .role = SemanticRole::Button,
                  .label = app::strings::add_attachment,
                  .descendants = SemanticDescendantPolicy::Exclude,
              }
          )
  );
  if (!picker->CanOpenFiles()) {
    attachment_controls.Add(
        Text(app::strings::attachment_picker_unavailable).Style({Font::System(12.0F), theme.colors.on_surface_variant})
    );
  }

  View send_button = Button(sending ? sending_label : send_label).With(Enabled(!sending)).OnClick([=] {
    interaction.composer_validation_attempted = true;
    const ComposerDraft current = interaction.composer.Get();
    if (ValidateRecipient(current.recipient).IsInvalid()) {
      return;
    }
    if (current.subject.empty()) {
      dialog.Show(
          app::strings::empty_subject_title,
          app::strings::empty_subject_message,
          app::strings::send_without_subject,
          app::strings::cancel,
          [=] {
            StartSend(data, navigation, interaction, app_tasks, service, toast, live_sending, live_sent, live_failed);
          },
          {},
          {.dismiss_on_outside_press = false}
      );
      return;
    }
    StartSend(data, navigation, interaction, app_tasks, service, toast, live_sending, live_sent, live_failed);
  });

  return Column{
      TopAppBar(
          draft.reply ? StringVariant(app::strings::composer_reply_title)
                      : StringVariant(app::strings::composer_new_title),
          std::move(leading)
      ),
      ScrollView{
          Column{
              std::move(recipient),
              std::move(subject),
              std::move(body),
              Row{
                  Text(app::strings::composer_attachments, TextRole::Title)
                      .Style({Font::System(16.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface})
                      .With(Grow()),
                  Row{std::move(attachment_controls)}.With(Spacing(10), CrossAlign(CrossAxisAlignment::Center)),
              }
                  .With(CrossAlign(CrossAxisAlignment::Center)),
              Column{std::move(attachments)}.With(Spacing(8), CrossAlign(CrossAxisAlignment::Stretch)),
              Row{
                  Spacer(),
                  std::move(send_button),
              },
              Text(interaction.live_announcement.Get())
                  .Style({Font::System(12.0F), theme.colors.on_surface_variant})
                  .With(
                      Semantics{
                          .busy = sending,
                          .live_region = SemanticLiveRegion::Polite,
                      }
                  ),
          }
              .With(
                  Frame{.max_width = 840.0F},
                  Spacing(20),
                  Padding(compact ? EdgeInsets::All(20) : EdgeInsets::All(38)),
                  Background(theme.colors.surface),
                  CornerRadius(compact ? 0.0F : theme.shapes.large),
                  Shadow(Color::Rgb(0, 0, 0, compact ? 0.0F : 0.055F), {0, 0}, compact ? 0.0F : 36.0F, 0),
                  Align(HorizontalAlignment::Center, VerticalAlignment::Start),
                  CrossAlign(CrossAxisAlignment::Stretch)
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
          Transition{AnimateTo(entered.Get() ? 1.0F : 0.0F, TweenSpec{theme.motion.normal, Easing::EaseOut})}
              .Opacity(0.0F, 1.0F)
              .Offset({0.0F, theme.motion.reduced_motion ? 0.0F : 14.0F}, {})
      );
}

} // namespace huxer_mail
