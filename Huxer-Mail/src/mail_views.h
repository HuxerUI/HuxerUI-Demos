#pragma once

#include <string>

#include <huxerui/environment.h>
#include <huxerui/view.h>

#include "mail_model.h"

namespace huxer_mail {

using namespace huxerui;

[[nodiscard]] View MailboxRoot();
[[nodiscard]] View ReaderView(std::string thread_id, bool compact);
[[nodiscard]] View ComposerView(bool compact);
[[nodiscard]] View SettingsView(bool compact);

} // namespace huxer_mail
