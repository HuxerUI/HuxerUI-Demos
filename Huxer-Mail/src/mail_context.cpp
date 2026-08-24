#include "mail_context.h"

#include <algorithm>

namespace huxer_mail {

using namespace huxerui;

std::vector<MailThread> SnapshotThreads(const StateList<MailThread>& threads) {
  return {threads.begin(), threads.end()};
}

std::optional<MailThread> FindThread(const StateList<MailThread>& threads, std::string_view id) {
  for (const MailThread& thread : threads) {
    if (thread.id == id) {
      return thread;
    }
  }
  return std::nullopt;
}

bool UpdateThread(
    const StateList<MailThread>& threads, std::string_view id, const std::function<void(MailThread&)>& update
) {
  for (std::size_t index = 0; index < threads.Size(); ++index) {
    if (threads[index].id == id) {
      MailThread value = threads[index];
      update(value);
      threads.Set(index, std::move(value));
      return true;
    }
  }
  return false;
}

bool EraseThread(const StateList<MailThread>& threads, std::string_view id) {
  for (std::size_t index = 0; index < threads.Size(); ++index) {
    if (threads[index].id == id) {
      threads.Erase(index);
      return true;
    }
  }
  return false;
}

bool ContainsId(const std::vector<std::string>& values, std::string_view id) {
  return std::ranges::find(values, id) != values.end();
}

std::vector<MailThread> ProjectThreads(
    const MailboxDataContext& data, const NavigationContext& navigation, const InteractionContext& interaction
) {
  const bool searching = !interaction.search.Get().text.empty();
  const std::vector<std::string>& matches = interaction.search_results.Get();
  std::vector<MailThread> projected;
  for (const MailThread& thread : data.threads) {
    const bool visible =
        searching ? ContainsId(matches, thread.id) : IsVisibleInFolder(thread, navigation.folder.Get());
    if (visible && MatchesFilter(thread, navigation.filter.Get())) {
      projected.push_back(thread);
    }
  }
  std::ranges::sort(projected, [](const MailThread& left, const MailThread& right) {
    return left.relative_minutes < right.relative_minutes;
  });
  return projected;
}

std::size_t InboxUnreadCount(const StateList<MailThread>& threads) {
  return static_cast<std::size_t>(std::ranges::count_if(threads, [](const MailThread& thread) {
    return thread.folder == MailFolder::Inbox && thread.unread;
  }));
}

std::size_t FolderCount(const StateList<MailThread>& threads, MailFolder folder) {
  return static_cast<std::size_t>(std::ranges::count_if(threads, [folder](const MailThread& thread) {
    return IsVisibleInFolder(thread, folder);
  }));
}

void ToggleSelected(const InteractionContext& interaction, std::string id) {
  interaction.selected_ids.Update([&id](std::vector<std::string>& selected) {
    const auto iterator = std::ranges::find(selected, id);
    if (iterator == selected.end()) {
      selected.push_back(std::move(id));
    } else {
      selected.erase(iterator);
    }
  });
}

void ClearSelection(const InteractionContext& interaction) {
  interaction.selection_mode = false;
  interaction.selected_ids = std::vector<std::string>{};
}

void ToggleStar(const MailboxDataContext& data, std::string_view thread_id) {
  (void)UpdateThread(data.threads, thread_id, [](MailThread& thread) { thread.starred = !thread.starred; });
}

void SetUnread(const MailboxDataContext& data, std::string_view thread_id, bool unread) {
  (void)UpdateThread(data.threads, thread_id, [unread](MailThread& thread) { thread.unread = unread; });
}

void ApplyMoveWithUndo(
    const MailboxDataContext& data,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    std::string_view thread_id,
    MailFolder destination,
    UndoKind kind
) {
  const std::optional<MailThread> thread = FindThread(data.threads, thread_id);
  if (!thread) {
    return;
  }
  const bool was_selected = ContainsId(interaction.selected_ids.Get(), thread_id);
  interaction.undo_sequence += 1;
  interaction.undo = UndoRecord{
      .token = interaction.undo_sequence.Get(),
      .items = {{
          .thread_id = std::string(thread_id),
          .previous_folder = thread->folder,
          .previous_unread = thread->unread,
          .previous_selected = was_selected,
      }},
      .kind = kind,
  };
  (void)UpdateThread(data.threads, thread_id, [destination](MailThread& value) {
    value.folder = destination;
    value.unread = false;
  });
  if (navigation.focused_thread.Get() == std::optional<std::string>(std::string(thread_id))) {
    navigation.focused_thread = std::nullopt;
    navigation.path = NavigationPath<MailRoute>{};
  }
  interaction.selected_ids.Update([thread_id](std::vector<std::string>& selected) { std::erase(selected, thread_id); });
}

void ApplyBulkMove(
    const MailboxDataContext& data,
    const NavigationContext& navigation,
    const InteractionContext& interaction,
    MailFolder destination,
    UndoKind kind
) {
  const std::vector<std::string> selected = interaction.selected_ids.Get();
  std::vector<UndoItem> items;
  for (const std::string& id : selected) {
    if (const std::optional<MailThread> thread = FindThread(data.threads, id)) {
      items.push_back({
          .thread_id = id,
          .previous_folder = thread->folder,
          .previous_unread = thread->unread,
          .previous_selected = true,
      });
      (void)UpdateThread(data.threads, id, [destination](MailThread& value) {
        value.folder = destination;
        value.unread = false;
      });
      if (navigation.focused_thread.Get() == std::optional<std::string>(id)) {
        navigation.focused_thread = std::nullopt;
        navigation.path = NavigationPath<MailRoute>{};
      }
    }
  }
  if (!items.empty()) {
    interaction.undo_sequence += 1;
    interaction.undo = UndoRecord{
        .token = interaction.undo_sequence.Get(),
        .items = std::move(items),
        .kind = kind,
    };
  }
  ClearSelection(interaction);
}

void ApplyBulkRead(const MailboxDataContext& data, const InteractionContext& interaction) {
  for (const std::string& id : interaction.selected_ids.Get()) {
    SetUnread(data, id, false);
  }
  ClearSelection(interaction);
}

void UndoLastMove(
    const MailboxDataContext& data, const NavigationContext& navigation, const InteractionContext& interaction
) {
  const std::optional<UndoRecord> undo = interaction.undo.Get();
  if (!undo) {
    return;
  }
  for (const UndoItem& item : undo->items) {
    (void)UpdateThread(data.threads, item.thread_id, [item](MailThread& thread) {
      thread.folder = item.previous_folder;
      thread.unread = item.previous_unread;
    });
    if (item.previous_selected) {
      interaction.selected_ids.Update([item](std::vector<std::string>& selected) {
        if (!ContainsId(selected, item.thread_id)) {
          selected.push_back(item.thread_id);
        }
      });
    }
  }
  if (!undo->items.empty()) {
    navigation.folder = undo->items.front().previous_folder;
    if (std::ranges::any_of(undo->items, [](const UndoItem& item) { return item.previous_selected; })) {
      interaction.selection_mode = true;
    }
  }
  interaction.undo = std::nullopt;
}

void SaveComposerDraft(const MailboxDataContext& data, const InteractionContext& interaction) {
  const ComposerDraft draft = interaction.composer.Get();
  if (draft.Empty()) {
    return;
  }
  bool draft_replaced = false;
  for (std::size_t index = 0; index < data.drafts.Size(); ++index) {
    if (data.drafts[index].id == draft.id) {
      data.drafts.Set(index, draft);
      draft_replaced = true;
      break;
    }
  }
  if (!draft_replaced) {
    data.drafts.PushBack(draft);
  }
  MailMessage message{
      .id = draft.id + "-message",
      .sender_name = "You",
      .sender_email = "you@huxermail.test",
      .recipient_name = draft.recipient,
      .recipient_email = draft.recipient,
      .body = draft.body,
      .direction = MessageDirection::Outgoing,
      .attachments = draft.attachments,
  };
  MailThread draft_thread{
      .id = draft.id,
      .subject = draft.subject,
      .sender_name = draft.recipient,
      .sender_email = draft.recipient,
      .excerpt = draft.body,
      .folder = MailFolder::Drafts,
      .messages = {std::move(message)},
  };
  if (!UpdateThread(data.threads, draft.id, [&draft_thread](MailThread& thread) { thread = draft_thread; })) {
    data.threads.PushBack(std::move(draft_thread));
  }
}

void CompleteSend(const MailboxDataContext& data, const InteractionContext& interaction, const ComposerDraft& draft) {
  MailMessage outgoing{
      .id = draft.id + "-sent-message",
      .sender_name = "You",
      .sender_email = "you@huxermail.test",
      .recipient_name = draft.recipient,
      .recipient_email = draft.recipient,
      .body = draft.body,
      .direction = MessageDirection::Outgoing,
      .attachments = draft.attachments,
  };

  if (draft.thread_id) {
    (void)UpdateThread(data.threads, *draft.thread_id, [&outgoing](MailThread& thread) {
      thread.messages.push_back(outgoing);
      thread.excerpt = outgoing.body;
      thread.relative_minutes = 0;
      thread.unread = false;
    });
  }

  data.threads.PushBack(
      MailThread{
          .id = draft.id + "-sent",
          .subject = draft.subject,
          .sender_name = draft.recipient,
          .sender_email = draft.recipient,
          .excerpt = draft.body,
          .folder = MailFolder::Sent,
          .messages = {std::move(outgoing)},
      }
  );
  for (std::size_t index = 0; index < data.drafts.Size(); ++index) {
    if (data.drafts[index].id == draft.id) {
      data.drafts.Erase(index);
      break;
    }
  }
  (void)EraseThread(data.threads, draft.id);
  interaction.composer = ComposerDraft{};
}

void CompleteOutboxRetry(const MailboxDataContext& data, std::string_view thread_id) {
  (void)UpdateThread(data.threads, thread_id, [](MailThread& thread) {
    thread.failed = false;
    thread.folder = MailFolder::Sent;
    thread.relative_minutes = 0;
  });
}

void AddSystemNotification(const MailboxDataContext& data, MailThread notification) {
  data.threads.Insert(0, std::move(notification));
}

} // namespace huxer_mail
