#pragma once

#include <string>
#include <vector>

#include <huxerui/task.h>

#include "mail_model.h"

namespace huxer_mail {

using namespace huxerui;

struct RefreshResult {
  bool succeeded = false;
  bool should_add_notification = false;
};

class MockMailService final {
public:
  [[nodiscard]] Task<void> CompleteStartupSync() const;
  [[nodiscard]] Task<std::vector<std::string>> Search(std::vector<MailThread> threads, std::string query) const;
  [[nodiscard]] Task<RefreshResult> Refresh(bool should_fail, bool should_add_notification) const;
  [[nodiscard]] Task<bool> Send() const;
  [[nodiscard]] Task<bool> RetryOutbox() const;
};

} // namespace huxer_mail
