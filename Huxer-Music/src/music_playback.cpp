#include "music_playback.h"
#include <app_resources.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace huxer_music {

std::string TimeLabel(double seconds) {
  const auto value = static_cast<long long>(std::max(0.0, std::isfinite(seconds) ? seconds : 0.0));
  std::ostringstream text;
  text << value / 60 << ':' << std::setw(2) << std::setfill('0') << value % 60;
  return text.str();
}


void SelectTrack(const MusicContext& music, std::size_t index, bool autoplay) {
  if (!music.ready.Get() || index >= music.tracks.Get().size()) return;
  const auto source = music.tracks.Get()[index].source;
  if (!source) return;
  music.error = std::string{};
  music.index = index;
  if (!music.player->Load(*source)) { music.error = "The audio player is unavailable."; return; }
  if (autoplay) (void)music.player->Play();
}

void TogglePlayback(const MusicContext& music) {
  const auto state = music.player->Snapshot();
  if (state.status == MediaPlayerStatus::Failed || state.status == MediaPlayerStatus::Empty) {
    SelectTrack(music, music.index.Get());
  } else if (state.play_when_ready) (void)music.player->Pause();
  else (void)music.player->Play();
}

void OpenAudio(const MusicContext& music) {
  if (music.picking.Get() || !music.ready.Get()) return;
  music.picking = true;
  (void)music.tasks.Launch([music]() -> Task<void> {
    try {
      auto file = co_await music.picker->OpenFileAsync({.name = "Audio", .content_types = {"audio/*"}});
      if (file) {
        auto tracks = music.tracks.Get();
        Track track = tracks.front();
        track.title = file->Name();
        track.artist = "Local audio";
        track.collection = "Your device";
        track.duration = "--:--";
        track.source = std::make_shared<MediaSource>(*file);
        track.local = true;
        tracks.push_back(std::move(track));
        const auto index = tracks.size() - 1;
        music.tracks = std::move(tracks);
        SelectTrack(music, index);
      }
    } catch (const std::exception&) { music.error = "Could not open this audio file. Please choose it again."; }
    music.picking = false;
  });
}

MusicContext UseMusic() {
  const auto player = UseMediaPlayer();
  MusicContext music{
      .player = UseState(std::make_shared<MediaPlayer>(player)).Get(),
      .tracks = UseState(std::vector<Track>{
          {.title = "Good to Go", .artist = "Josh Woodward", .collection = "The Simple Life",
           .background = Color::Rgb(13, 18, 31), .accent = Color::Rgb(117, 92, 255), .secondary = Color::Rgb(250, 102, 143)},
          {.title = "On Brevity", .artist = "Josh Woodward", .collection = "The Simple Life",
           .background = Color::Rgb(11, 25, 28), .accent = Color::Rgb(48, 202, 170), .secondary = Color::Rgb(88, 141, 255)},
          {.title = "Dizzy Spells", .artist = "Josh Woodward", .collection = "Not Quite Connected",
           .background = Color::Rgb(30, 15, 20), .accent = Color::Rgb(255, 126, 78), .secondary = Color::Rgb(244, 71, 134)}
      }),
      .index = UseState(std::size_t{0}), .playing = UseState(false), .ready = UseState(false),
      .picking = UseState(false), .error = UseState(std::string{}), .tasks = UseTaskScope(), .picker = UseService<FilePicker>()};
  const std::vector<RawAsset> assets{UseRawResource(app::raw::good_to_go_mp3), UseRawResource(app::raw::on_brevity_mp3), UseRawResource(app::raw::dizzy_spells_mp3)};
  const auto directory = UseApplication().Directories().data_directory.Child("bundled-audio");
  Lifecycle([=] {
    (void)music.tasks.Launch([=]() -> Task<void> {
      try {
        if (!co_await directory.CreateDirectoriesAsync()) throw std::runtime_error("Audio directory unavailable");
        auto tracks = music.tracks.Get();
        for (std::size_t index = 0; index < assets.size(); ++index) {
          const auto file = directory.Child("track-" + std::to_string(index) + ".mp3");
          auto input = co_await assets[index].OpenReadAsync();
          auto output = co_await file.OpenWriteAsync();
          if (!output.Succeeded()) throw std::runtime_error("Audio file unavailable");
          auto copied = co_await input.CopyToAsync(output.Value(), 64 * 1024);
          auto closed = co_await output.Value().CloseAsync();
          if (!copied.Succeeded() || !closed.Succeeded()) throw std::runtime_error("Audio preparation failed");
          tracks[index].source = std::make_shared<MediaSource>(file);
        }
        music.tracks = std::move(tracks);
        music.ready = true;
        (void)music.player->SetVolume(0.68);
        SelectTrack(music, 0, false);
      } catch (const std::exception&) { music.error = "Could not prepare the bundled songs. Please reopen the app."; }
    });
  });
  player.OnProgress([music](const MediaProgress& progress) {
    const auto state = music.player->Snapshot();
    music.playing = state.status == MediaPlayerStatus::Playing && !state.is_buffering;
    if (progress.duration && music.index.Get() < music.tracks.Get().size()) {
      const auto label = TimeLabel(progress.duration->count());
      if (music.tracks.Get()[music.index.Get()].duration != label)
        music.tracks.Update([&](auto& tracks) { tracks[music.index.Get()].duration = label; });
    }
  });
  player.OnEnded([music] { SelectTrack(music, (music.index.Get() + 1) % music.tracks.Get().size()); });
  player.OnError([music](const MediaError& error) { music.error = error.message.empty() ? "Audio playback failed." : error.message; });
  return music;
}

[[huxerui::composable]]
View PlaybackNotice() {
  const auto music = UseEnvironment<MusicContext>();
  const auto state = music.player->Snapshot();
  std::string message = music.error.Get();
  if (message.empty()) {
    if (!music.ready.Get()) message = "Preparing offline songs…";
    else if (state.is_seeking) message = "Seeking…";
    else if (state.is_buffering || state.status == MediaPlayerStatus::Loading) message = "Loading audio…";
    else if (state.status == MediaPlayerStatus::Interrupted) message = "Playback interrupted";
  }
  if (message.empty()) return View();
  return Row{Text(message).Style({Font::System(12), Color::Rgb(230, 230, 240)}).With(Grow()),
      state.status == MediaPlayerStatus::Failed ? View(Button("Retry").OnClick([music] { SelectTrack(music, music.index.Get()); })) : View()}
      .With(Spacing(10), Padding(10), CrossAlign(CrossAxisAlignment::Center));
}
} // namespace huxer_music
