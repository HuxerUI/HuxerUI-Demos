#pragma once

#include <huxerui/theme.h>

#include "mail_model.h"

namespace huxer_mail {

using namespace huxerui;

[[nodiscard]] ThemeDefinition MailThemeDefinition(ThemeMode mode, bool reduced_motion);
[[nodiscard]] CheckboxStyle MailCheckboxStyle(const ThemeSpec& theme);

} // namespace huxer_mail
