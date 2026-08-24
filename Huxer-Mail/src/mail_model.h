#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <huxerui/file.h>

namespace huxer_mail {

using namespace huxerui;

enum class MailFolder {
  Inbox,
  Starred,
  Sent,
  Drafts,
  Outbox,
  Trash,
  Archive,
};

enum class MailFilter {
  All,
  Unread,
  Starred,
};

enum class MessageDirection {
  Incoming,
  Outgoing,
};

enum class AttachmentOrigin {
  Mock,
  Picked,
};

struct MailAttachment {
  std::string id;
  std::string name;
  std::uint64_t size = 0;
  std::string content_type;
  AttachmentOrigin origin = AttachmentOrigin::Mock;
  std::optional<FileReference> reference;

  bool operator==(const MailAttachment& other) const {
    return id == other.id && name == other.name && size == other.size && content_type == other.content_type &&
           origin == other.origin;
  }
};

struct MailMessage {
  std::string id;
  std::string sender_name;
  std::string sender_email;
  std::string recipient_name;
  std::string recipient_email;
  std::string body;
  int relative_minutes = 0;
  MessageDirection direction = MessageDirection::Incoming;
  std::vector<MailAttachment> attachments;

  bool operator==(const MailMessage&) const = default;
};

struct MailThread {
  std::string id;
  std::string subject;
  std::string sender_name;
  std::string sender_email;
  std::string excerpt;
  int relative_minutes = 0;
  MailFolder folder = MailFolder::Inbox;
  bool unread = false;
  bool starred = false;
  bool failed = false;
  std::vector<MailMessage> messages;

  bool operator==(const MailThread&) const = default;
};

struct ComposerDraft {
  std::string id;
  std::optional<std::string> thread_id;
  std::string recipient;
  std::string subject;
  std::string body;
  std::vector<MailAttachment> attachments;
  bool reply = false;

  [[nodiscard]] bool Empty() const {
    return recipient.empty() && subject.empty() && body.empty() && attachments.empty();
  }

  bool operator==(const ComposerDraft&) const = default;
};

enum class UndoKind {
  Archive,
  Delete,
};

struct UndoItem {
  std::string thread_id;
  MailFolder previous_folder = MailFolder::Inbox;
  bool previous_unread = false;
  bool previous_selected = false;

  bool operator==(const UndoItem&) const = default;
};

struct UndoRecord {
  std::uint64_t token = 0;
  std::vector<UndoItem> items;
  UndoKind kind = UndoKind::Archive;

  bool operator==(const UndoRecord&) const = default;
};

enum class SyncPhase {
  Local,
  Syncing,
  Synced,
  Failed,
};

struct SyncStatus {
  SyncPhase phase = SyncPhase::Local;
  bool first_manual_attempt_consumed = false;
  bool retry_available = false;
  bool notification_added = false;

  bool operator==(const SyncStatus&) const = default;
};

enum class ThemeMode {
  Light,
  Dark,
};

enum class MailRouteKind {
  Reader,
  Composer,
  Settings,
};

struct MailRoute {
  MailRouteKind kind = MailRouteKind::Reader;
  std::string value;

  bool operator==(const MailRoute&) const = default;
};

[[nodiscard]] std::vector<MailThread> BuildMockThreads();
[[nodiscard]] std::optional<std::size_t> FindThreadIndex(const std::vector<MailThread>& threads, std::string_view id);
[[nodiscard]] bool MatchesSearch(const MailThread& thread, std::string_view normalized_query);
[[nodiscard]] std::string NormalizeSearch(std::string_view value);
[[nodiscard]] bool IsVisibleInFolder(const MailThread& thread, MailFolder folder);
[[nodiscard]] bool MatchesFilter(const MailThread& thread, MailFilter filter);

} // namespace huxer_mail
