#pragma once

#include <huxerui/huxerui.h>

namespace huxer_music {

struct AlbumMotion {
  class Extension;

  static const huxerui::detail::ModifierDescriptor& Descriptor();

  bool playing = true;
  huxerui::Color accent;
  huxerui::Color secondary;
  float corner_radius = 28.0F;

  bool operator==(const AlbumMotion&) const = default;
};

struct AmbientMotion {
  class Extension;

  static const huxerui::detail::ModifierDescriptor& Descriptor();

  bool playing = true;
  huxerui::Color accent;
  huxerui::Color secondary;

  bool operator==(const AmbientMotion&) const = default;
};

struct EqualizerMotion {
  class Extension;

  static const huxerui::detail::ModifierDescriptor& Descriptor();

  bool playing = true;
  huxerui::Color accent;
  huxerui::Color secondary;

  bool operator==(const EqualizerMotion&) const = default;
};

} // namespace huxer_music
