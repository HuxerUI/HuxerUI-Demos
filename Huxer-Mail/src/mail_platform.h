#pragma once

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace huxer_mail {

inline constexpr bool kUsesDesktopWindowChrome =
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
    false;
#else
    true;
#endif

} // namespace huxer_mail
