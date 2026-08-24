#include "mock_mail_service.h"

#include <chrono>

namespace huxer_mail {

using namespace huxerui;
using namespace std::chrono_literals;

Task<void> MockMailService::CompleteStartupSync() const {
  co_await Delay(620ms);
}

Task<std::vector<std::string>> MockMailService::Search(std::vector<MailThread> threads, std::string query) const {
  co_await Delay(180ms);
  const std::string normalized = NormalizeSearch(query);
  std::vector<std::string> matches;
  for (const MailThread& thread : threads) {
    if (MatchesSearch(thread, normalized)) {
      matches.push_back(thread.id);
    }
  }
  co_return matches;
}

Task<RefreshResult> MockMailService::Refresh(bool should_fail, bool should_add_notification) const {
  co_await Delay(760ms);
  co_return RefreshResult{
      .succeeded = !should_fail,
      .should_add_notification = !should_fail && should_add_notification,
  };
}

Task<bool> MockMailService::Send() const {
  co_await Delay(720ms);
  co_return true;
}

Task<bool> MockMailService::RetryOutbox() const {
  co_await Delay(680ms);
  co_return true;
}

} // namespace huxer_mail
