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

ThemeDefinition ComposerFieldTheme(const ThemeSpec& theme, float text_size, FontWeight weight) {
  TextFieldStyle style = TextFieldStyle::Default();
  style.variant = TextFieldVariant::Standard;
  style.show_label = false;
  style.standard.background = Color::Transparent();
  style.standard.border = Color::Transparent();
  style.standard.hovered_border = Color::Transparent();
  style.standard.focused_border = Color::Transparent();
  style.standard.disabled_border = theme.colors.outline;
  style.standard.corner_radii = CornerRadii{};
  style.standard.minimum_height = 48.0F;
  style.text_style = TextStyle{Font::System(text_size).WithWeight(weight), theme.colors.on_surface};
  style.label_style =
      TextStyle{Font::System(12.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface_variant};
  style.floating_label_style = style.label_style;
  style.placeholder_style = TextStyle{Font::System(text_size), theme.colors.on_surface_variant};
  style.focused_label = theme.colors.primary;
  style.selection = Color::Rgb(116, 87, 245, 0.20F);
  style.caret = theme.colors.primary;
  style.composition = theme.colors.primary;
  style.padding = EdgeInsets::Symmetric(0.0F, 10.0F);
  ThemeDefinition definition;
  definition.Set(style);
  return definition;
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

  View close_button = IconButton(app::images::close, app::strings::close).OnClick([=] {
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
            Text(attachment.name)
                .Style({Font::System(13.0F).WithWeight(FontWeight::Medium), theme.colors.on_surface}),
            std::move(size_text).With(Grow()),
            IconButton(app::images::close, app::strings::remove_attachment)
                .OnClick([interaction, id = attachment.id] {
                  interaction.composer.Update([&id](ComposerDraft& value) {
                    std::erase_if(value.attachments, [&id](const MailAttachment& item) { return item.id == id; });
                  });
                })
                .With(Tooltip(StringVariant::Format(app::strings::attachment_remove_hint, attachment.name))),
        }
            .With(
                Spacing(10),
                Padding(EdgeInsets::Symmetric(12, 6)),
                Background(theme.colors.surface_container),
                Border{theme.colors.outline, 1.0F},
                CornerRadius(theme.shapes.small),
                CrossAlign(CrossAxisAlignment::Center),
                Frame{.max_width = 420.0F},
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
                IoResult<Bytes> content = co_await reference.ReadBytesAsync();
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
      Row{
          Column{
              Text(draft.reply ? StringVariant(app::strings::composer_reply_title)
                               : StringVariant(app::strings::composer_new_title))
                  .Style({Font::System(20.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
              Text(app::strings::composer_saved)
                  .Style({Font::System(12.0F), theme.colors.on_surface_variant}),
          }
              .With(Spacing(3), Grow()),
          std::move(close_button),
      }
          .With(
              Padding(EdgeInsets::Symmetric(compact ? 18.0F : 28.0F, 14.0F)),
              CrossAlign(CrossAxisAlignment::Center),
              Frame{.height = compact ? 64.0F : 78.0F}
          ),
      Divider(),
      ScrollView{
          Column{
              Row{
                  Text(app::strings::recipient_label)
                      .Style({Font::System(13.0F).WithWeight(FontWeight::Medium),
                              theme.colors.on_surface_variant})
                      .With(Frame{.width = 74.0F}),
                  Column{
                      Theme(ComposerFieldTheme(theme, 15.0F, FontWeight::Regular), std::move(recipient)),
                  }
                      .With(Grow(), CrossAlign(CrossAxisAlignment::Stretch)),
              }
                  .With(CrossAlign(CrossAxisAlignment::Center), Frame{.min_height = 58.0F}),
              Divider(),
              Row{
                  Text(app::strings::subject_label)
                      .Style({Font::System(13.0F).WithWeight(FontWeight::Medium),
                              theme.colors.on_surface_variant})
                      .With(Frame{.width = 74.0F}),
                  Column{
                      Theme(ComposerFieldTheme(theme, 18.0F, FontWeight::SemiBold), std::move(subject)),
                  }
                      .With(Grow(), CrossAlign(CrossAxisAlignment::Stretch)),
              }
                  .With(CrossAlign(CrossAxisAlignment::Center), Frame{.min_height = 58.0F}),
              Divider(),
              Theme(ComposerFieldTheme(theme, 16.0F, FontWeight::Regular), std::move(body)),
              Spacer().With(Frame{.height = compact ? 8.0F : 20.0F}),
              Column{std::move(attachments)}.With(Spacing(8), CrossAlign(CrossAxisAlignment::Stretch)),
          }
              .With(
                  Frame{.max_width = 820.0F},
                  Spacing(0),
                  Padding(compact ? EdgeInsets::All(18) : EdgeInsets{22, 34, 26, 34}),
                  Align(HorizontalAlignment::Center, VerticalAlignment::Start),
                  CrossAlign(CrossAxisAlignment::Stretch)
              ),
      }
          .With(ScrollBar(), Background(theme.colors.surface), Grow()),
      Divider(),
      Row{
          std::move(send_button).With(Frame{.width = 96.0F, .height = 40.0F}),
          Row{std::move(attachment_controls)}.With(Spacing(8), CrossAlign(CrossAxisAlignment::Center)),
          Text(interaction.live_announcement.Get())
              .Style({Font::System(12.0F), theme.colors.on_surface_variant})
              .With(
                  Grow(),
                  Semantics{
                      .busy = sending,
                      .live_region = SemanticLiveRegion::Polite,
                  }
              ),
      }
          .With(
              Spacing(12),
              Padding(EdgeInsets::Symmetric(compact ? 18.0F : 28.0F, 12.0F)),
              CrossAlign(CrossAxisAlignment::Center),
              Background(theme.colors.surface_container),
              Frame{.height = compact ? 62.0F : 68.0F}
          ),
  }
      .With(
          Background(theme.colors.surface),
          Grow(),
          Transition{AnimateTo(entered.Get() ? 1.0F : 0.0F, TweenSpec{theme.motion.normal, Easing::EaseOut})}
              .Opacity(0.0F, 1.0F)
              .Offset({0.0F, theme.motion.reduced_motion ? 0.0F : 8.0F}, {})
      );
}

} // namespace huxer_mail
