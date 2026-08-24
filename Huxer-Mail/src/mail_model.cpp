#include "mail_model.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace huxer_mail {
namespace {

struct Contact {
  std::string_view name;
  std::string_view email;
};

constexpr std::array<Contact, 12> kContacts{{
    {"Lena Chen", "lena.chen@northlake.test"},
    {"Maya Reed", "maya.reed@northlake.test"},
    {"Jonah Wells", "jonah.wells@northlake.test"},
    {"Elliot Park", "elliot.park@northlake.test"},
    {"Rina Patel", "rina.patel@northlake.test"},
    {"Noah Kim", "noah.kim@northlake.test"},
    {"Anika Green", "anika.green@northlake.test"},
    {"Iris Bell", "iris.bell@northlake.test"},
    {"Gavin Yu", "gavin.yu@northlake.test"},
    {"Owen Hart", "owen.hart@northlake.test"},
    {"Helen Young", "helen.young@northlake.test"},
    {"System Desk", "updates@northlake.test"},
}};

constexpr std::array<std::string_view, 28> kSubjects{{
    "Summer release readiness checklist",
    "Design review · workspace navigation",
    "Research summary · focus mode discovery",
    "Weekly project progress and risks",
    "Please approve the field study brief",
    "September workshop travel confirmation",
    "Partner preview feedback",
    "Purchase request · additional test devices",
    "Daily automation report is ready",
    "Final content guidelines review",
    "Release notes ready for review",
    "User interview schedule update",
    "Prototype handoff and open questions",
    "Additional travel expense documents",
    "Quarterly planning notes",
    "Brand illustration direction",
    "Invoice 0823 · confirmation needed",
    "Analytics dashboard access",
    "Workshop recap and decisions",
    "Friday demo rehearsal schedule",
    "Archive taxonomy proposal",
    "Research participant recruitment update",
    "Roadmap wording review",
    "Meeting room change",
    "Localization review · nine locales",
    "Partner information package updated",
    "Accessibility findings follow-up",
    "Build health summary",
}};

constexpr std::array<std::string_view, 20> kBodies{{
    "I reorganized the pre-release checklist. Please focus on the migration prompt, offline fallback, and release notes, "
    "then send me your conclusion by the end of the day.",
    "The navigation pass is ready. The hierarchy now favors quick triage while keeping secondary actions quiet. I left "
    "two questions in the review notes.",
    "The key finding is that people understand the value of focus mode, but its entry point needs to sit closer to "
    "their actual tasks. The full observation notes are attached.",
    "Progress is steady this week. Current risks are limited to resource review and mobile focus order, and both have "
    "clear closure dates.",
    "Could you approve the brief before the participant invitation goes out? The scope is unchanged and the consent "
    "language has been reviewed.",
    "The itinerary now reflects the latest meeting time. The train and hotel remain refundable; please confirm "
    "whether the return time should stay unchanged.",
    "The partner team liked the calmer visual direction. Their only request is a clearer empty state for first-time "
    "users.",
    "Our current devices do not cover small high-resolution screens, so I requested two additional units. The quote "
    "and usage notes are attached.",
    "Today's automation run completed all 18 items without blockers. You can export the detailed record from the "
    "attachment.",
    "Body copy, buttons, and error messages have passed final review. Please avoid introducing new terminology during "
    "this localization round.",
    "Release notes are ready. I simplified the opening section and moved technical caveats into a dedicated "
    "compatibility paragraph.",
    "Two participants rescheduled, so I reserved the open time for an internal review. The updated schedule will not "
    "affect delivery.",
    "The handoff package includes interaction notes, responsive states, and the original vector sources. Open "
    "questions are grouped at the top.",
    "Finance needs payment evidence for one hotel invoice. Everything else has been approved, and reimbursement can "
    "finish this week.",
    "I consolidated the planning notes around three outcomes: clarity, recovery, and offline confidence. Please add "
    "comments before Tuesday.",
    "The illustrations will keep their low-saturation geometric language without portrait photography. The next pass "
    "only needs a scale and dark-theme contrast review.",
    "Please confirm the invoice amount and project code. Once approved, the payment will be scheduled for the next "
    "processing window.",
    "Dashboard access is active and includes aggregate data only. Submit a separate access rationale if you need the "
    "raw interview notes.",
    "The workshop ended with four decisions and one follow-up experiment. I captured owners and dates in the attached "
    "recap.",
    "The rehearsal is scheduled for Friday at 15:30 and should take 35 minutes. Please arrive ten minutes early and "
    "use the offline dataset for the demo.",
}};

constexpr std::array<std::string_view, 8> kAttachmentNames{{
    "workspace-preview.svg",
    "navigation-notes.txt",
    "research-summary.pdf",
    "weekly-status.txt",
    "field-study-brief.pdf",
    "travel-plan.txt",
    "partner-feedback.pdf",
    "device-quote.pdf",
}};

MailAttachment MakeAttachment(std::size_t index) {
  return {
      .id = "attachment-" + std::to_string(index + 1),
      .name = std::string(kAttachmentNames[index % kAttachmentNames.size()]),
      .size = static_cast<std::uint64_t>(54'000 + index * 37'400),
      .content_type = index == 0 ? "image/svg+xml" : (index % 3 == 1 ? "text/plain" : "application/pdf"),
  };
}

MailThread MakeThread(std::size_t index, MailFolder folder, bool unread, bool starred, bool failed = false) {
  const Contact contact = kContacts[index % kContacts.size()];
  const std::string subject = std::string(kSubjects[index % kSubjects.size()]);
  const std::string body = std::string(kBodies[index % kBodies.size()]);
  const std::size_t message_count = index < 10 ? 2 + index % 3 : 1;

  MailThread thread{
      .id = "thread-" + std::to_string(index + 1),
      .subject = subject,
      .sender_name = std::string(contact.name),
      .sender_email = std::string(contact.email),
      .excerpt = body,
      .relative_minutes = static_cast<int>(12 + index * 47),
      .folder = folder,
      .unread = unread,
      .starred = starred,
      .failed = failed,
  };

  for (std::size_t message = 0; message < message_count; ++message) {
    const bool outgoing = message > 0 && message % 2 == 1;
    const Contact message_contact = kContacts[(index + message) % kContacts.size()];
    MailMessage value{
        .id = thread.id + "-message-" + std::to_string(message + 1),
        .sender_name = outgoing ? "You" : std::string(message_contact.name),
        .sender_email = outgoing ? "you@huxermail.test" : std::string(message_contact.email),
        .recipient_name = outgoing ? std::string(message_contact.name) : "You",
        .recipient_email = outgoing ? std::string(message_contact.email) : "you@huxermail.test",
        .body = message == 0 ? body
                             : "Thanks — I reviewed the latest notes. The direction is clear, and I added one concise "
                               "follow-up for the next pass.",
        .relative_minutes = thread.relative_minutes + static_cast<int>((message_count - message - 1) * 180),
        .direction = outgoing ? MessageDirection::Outgoing : MessageDirection::Incoming,
    };
    if (index < kAttachmentNames.size() && message + 1 == message_count) {
      value.attachments.push_back(MakeAttachment(index));
    }
    thread.messages.push_back(std::move(value));
  }
  return thread;
}

} // namespace

std::vector<MailThread> BuildMockThreads() {
  std::vector<MailThread> threads;
  threads.reserve(56);
  std::size_t index = 0;

  for (std::size_t i = 0; i < 30; ++i, ++index) {
    threads.push_back(MakeThread(index, MailFolder::Inbox, i < 8, i % 7 == 0));
  }
  for (std::size_t i = 0; i < 8; ++i, ++index) {
    threads.push_back(MakeThread(index, MailFolder::Sent, false, i == 2));
  }
  for (std::size_t i = 0; i < 3; ++i, ++index) {
    threads.push_back(MakeThread(index, MailFolder::Drafts, false, i == 1));
  }
  threads.push_back(MakeThread(index++, MailFolder::Outbox, false, false, true));
  for (std::size_t i = 0; i < 4; ++i, ++index) {
    threads.push_back(MakeThread(index, MailFolder::Trash, false, i == 0));
  }
  for (std::size_t i = 0; i < 10; ++i, ++index) {
    threads.push_back(MakeThread(index, MailFolder::Archive, false, i % 4 == 0));
  }
  return threads;
}

std::optional<std::size_t> FindThreadIndex(const std::vector<MailThread>& threads, std::string_view id) {
  const auto iterator =
      std::find_if(threads.begin(), threads.end(), [id](const MailThread& thread) { return thread.id == id; });
  if (iterator == threads.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(threads.begin(), iterator));
}

std::string NormalizeSearch(std::string_view value) {
  std::string normalized(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return normalized;
}

bool MatchesSearch(const MailThread& thread, std::string_view normalized_query) {
  if (normalized_query.empty()) {
    return true;
  }
  std::string searchable = thread.sender_name + " " + thread.sender_email + " " + thread.subject + " " + thread.excerpt;
  for (const MailMessage& message : thread.messages) {
    searchable += " " + message.body;
  }
  return NormalizeSearch(searchable).find(normalized_query) != std::string::npos;
}

bool IsVisibleInFolder(const MailThread& thread, MailFolder folder) {
  if (folder == MailFolder::Starred) {
    return thread.starred && thread.folder != MailFolder::Trash;
  }
  return thread.folder == folder;
}

bool MatchesFilter(const MailThread& thread, MailFilter filter) {
  switch (filter) {
  case MailFilter::All:
    return true;
  case MailFilter::Unread:
    return thread.unread;
  case MailFilter::Starred:
    return thread.starred;
  }
  return true;
}

} // namespace huxer_mail
