#pragma once

#include <huxerui/huxerui.h>

namespace huxer_music {

struct SpotlightHover {
  class Extension;

  static const huxerui::detail::ModifierDescriptor& Descriptor();

  float radius = 90.0F;
  float corner_radius = 36.0F;
  huxerui::Color hover_background = huxerui::Color::Rgb(39, 52, 73);

  bool operator==(const SpotlightHover&) const = default;
};

} // namespace huxer_music
