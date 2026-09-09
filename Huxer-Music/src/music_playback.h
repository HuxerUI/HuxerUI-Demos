#pragma once

#include <huxerui/huxerui.h>
#include <huxerui/mediaplayer.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace huxer_music {
using namespace huxerui;
using namespace huxerui::media;

struct Track {
  std::string title;
  std::string artist;
  std::string collection;
  std::string duration = "--:--";
  Color background;
  Color accent;
  Color secondary;
  std::shared_ptr<const MediaSource> source;
  bool local = false;
  bool operator==(const Track&) const = default;
};

struct MusicContext {
  std::shared_ptr<MediaPlayer> player;
  State<std::vector<Track>> tracks;
  State<std::size_t> index;
  State<bool> playing;
  State<bool> ready;
  State<bool> picking;
  State<std::string> error;
  TaskScope tasks;
  std::shared_ptr<FilePicker> picker;
  static MusicContext Default() { return {}; }
  bool operator==(const MusicContext& other) const { return player == other.player; }
};

MusicContext UseMusic();
void SelectTrack(const MusicContext& music, std::size_t index, bool autoplay = true);
void TogglePlayback(const MusicContext& music);
void OpenAudio(const MusicContext& music);
std::string TimeLabel(double seconds);
View PlaybackNotice();
} // namespace huxer_music
