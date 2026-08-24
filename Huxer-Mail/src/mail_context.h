#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <huxerui/app.h>
#include <huxerui/environment.h>
#include <huxerui/navigation.h>
#include <huxerui/state.h>
#include <huxerui/task.h>
#include <huxerui/text_input.h>

#include "mail_model.h"

namespace huxer_mail {

using namespace huxerui;

struct MailboxDataContext {
  StateList<MailThread> threads;
  StateList<ComposerDraft> drafts;
  std::shared_ptr<const void> identity;

  static MailboxDataContext Default() {
    return {};
  }

  bool operator==(const MailboxDataContext& other) const noexcept {
    return identity == other.identity;
  }
};

struct NavigationContext {
  State<MailFolder> folder;
  State<MailFilter> filter;
  State<NavigationPath<MailRoute>> path;
  State<std::optional<std::string>> focused_thread;
  State<bool> drawer_open;
  std::shared_ptr<const void> identity;

  static NavigationContext Default() {
    return {};
  }

  bool operator==(const NavigationContext& other) const noexcept {
    return identity == other.identity;
  }
};

struct InteractionContext {
  State<TextEditingValue> search;
  State<bool> search_loading;
  State<std::vector<std::string>> search_results;
  State<bool> selection_mode;
  State<std::vector<std::string>> selected_ids;
  State<ComposerDraft> composer;
  State<bool> composer_validation_attempted;
  State<bool> sending;
  State<std::vector<std::string>> attachment_errors;
  State<std::optional<UndoRecord>> undo;
  State<std::uint64_t> undo_sequence;
  State<std::vector<std::string>> expanded_messages;
  State<std::string> live_announcement;
  std::shared_ptr<const void> identity;

  static InteractionContext Default() {
    return {};
  }

  bool operator==(const InteractionContext& other) const noexcept {
    return identity == other.identity;
  }
};

struct AppStatusContext {
  State<SyncStatus> sync;
  State<ThemeMode> theme;
  State<bool> reduced_motion;
  State<ApplicationLifecycleState> lifecycle;
  State<int> clock_revision;
  std::shared_ptr<const void> identity;

  static AppStatusContext Default() {
    return {};
  }

  bool operator==(const AppStatusContext& other) const noexcept {
    return identity == other.identity;
  }
};

struct AppTaskContext {
  TaskScope scope;
  std::shared_ptr<const void> identity;

  static AppTaskContext Default() {
    return {};
  }

  bool operator==(const AppTaskContext& other) const noexcept {
    return identity == other.identity;
  }
};

[[nodiscard]] std::vector<MailThread> SnapshotThreads(const StateList<MailThread>& threads);
[[nodiscard]] std::optional<MailThread> FindThread(const StateList<MailThread>& threads, std::string_view id);
[[nodiscard]] bool
UpdateThread(const StateList<MailThread>& threads, std::string_view id, const std::function<void(MailThread&)>& update);
[[nodiscard]] bool EraseThread(const StateList<MailThread>& threads, std::string_view id);
[[nodiscard]] std::vector<MailThread> ProjectThreads(
    const MailboxDataContext& data, const NavigationContext& navigation, const InteractionContext& interaction
);
[[nodiscard]] std::size_t InboxUnreadCount(const StateList<MailThread>& threads);
[[nodiscard]] std::size_t FolderCount(const StateList<MailThread>& threads, MailFolder folder);
[[nodiscard]] bool ContainsId(const std::vector<std::string>& values, std::string_view id);
void ToggleSelected(const InteractionContext& interaction, std::string id);
void ClearSelection(const InteractionContext& interaction);
void ToggleStar(const MailboxDataContext& data, std::string_view thread_id);
void SetUnread(const MailboxDataContext& data, std::string_view thread_id, bool unread);
void ApplyMoveWithUndo(
    const MailboxDataContext& data,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    std::string_view thread_id,
    MailFolder destination,
    UndoKind kind
);
void ApplyBulkMove(
    const MailboxDataContext& data,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    MailFolder destination,
    UndoKind kind
);
void ApplyBulkRead(const MailboxDataContext& data, const InteractionContext& interaction);
void UndoLastMove(
    const MailboxDataContext& data, const NavigationContext& navigation, const InteractionContext& interaction
);
void SaveComposerDraft(const MailboxDataContext& data, const InteractionContext& interaction);
void CompleteSend(const MailboxDataContext& data, const InteractionContext& interaction, const ComposerDraft& draft);
void CompleteOutboxRetry(const MailboxDataContext& data, std::string_view thread_id);
void AddSystemNotification(const MailboxDataContext& data, MailThread notification);

} // namespace huxer_mail
