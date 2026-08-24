#include "mail_app.h"

#include <memory>

#include <app_resources.h>
#include <huxerui/huxerui.h>

#include "mail_context.h"
#include "mail_theme.h"
#include "mail_views.h"
#include "mock_mail_service.h"

namespace huxer_mail {

using namespace huxerui;

[[huxerui::composable]]
View HuxerMailApp() {
  const std::shared_ptr<const void> context_identity =
      UseState(std::shared_ptr<const void>(std::make_shared<int>(0))).Get();
  const MailboxDataContext data{
      .threads = UseStateList(BuildMockThreads()),
      .drafts = UseStateList<ComposerDraft>(),
      .identity = context_identity,
  };
  const NavigationContext navigation{
      .folder = UseState(MailFolder::Inbox),
      .filter = UseState(MailFilter::All),
      .path = UseState(NavigationPath<MailRoute>{}),
      .focused_thread = UseState(std::optional<std::string>{}),
      .drawer_open = UseState(false),
      .identity = context_identity,
  };
  const InteractionContext interaction{
      .search = UseState(TextEditingValue::FromText({})),
      .search_loading = UseState(false),
      .search_results = UseState(std::vector<std::string>{}),
      .selection_mode = UseState(false),
      .selected_ids = UseState(std::vector<std::string>{}),
      .composer = UseState(ComposerDraft{.id = "compose-session-1"}),
      .composer_validation_attempted = UseState(false),
      .sending = UseState(false),
      .attachment_errors = UseState(std::vector<std::string>{}),
      .undo = UseState(std::optional<UndoRecord>{}),
      .undo_sequence = UseState(std::uint64_t{0}),
      .expanded_messages = UseState(std::vector<std::string>{}),
      .live_announcement = UseState(std::string{}),
      .identity = context_identity,
  };
  const AppStatusContext status{
      .sync = UseState(SyncStatus{.phase = SyncPhase::Syncing}),
      .theme = UseState(ThemeMode::Light),
      .reduced_motion = UseState(false),
      .lifecycle = UseState(ApplicationLifecycleState::Active),
      .clock_revision = UseState(0),
      .identity = context_identity,
  };
  const AppTaskContext tasks{
      .scope = UseTaskScope(),
      .identity = context_identity,
  };
  const std::shared_ptr<MockMailService> service = UseService<MockMailService>();

  Lifecycle([service, tasks, sync = status.sync] {
    (void)tasks.scope.Launch([service, sync]() -> Task<void> {
      co_await service->CompleteStartupSync();
      sync.Update([](SyncStatus& value) {
        if (!value.first_manual_attempt_consumed && value.phase == SyncPhase::Syncing) {
          value.phase = SyncPhase::Synced;
        }
      });
    });
  });

  UseApplication().OnLifecycleChange([lifecycle = status.lifecycle,
                                      clock = status.clock_revision](ApplicationLifecycleState value) {
    lifecycle = value;
    if (value == ApplicationLifecycleState::Active) {
      clock += 1;
    }
  });

  Environment environment;
  environment.Set(data).Set(navigation).Set(interaction).Set(status).Set(tasks);
  return Theme(
      MailThemeDefinition(status.theme.Get(), status.reduced_motion.Get()),
      ProvideEnvironment(std::move(environment), MailboxRoot())
  );
}

} // namespace huxer_mail
