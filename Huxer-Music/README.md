# Huxer Music

Huxer Music is a HuxerUI music player demo with real audio playback through
[HuxerUI/Lib-MediaPlayer](https://github.com/HuxerUI/Lib-MediaPlayer).
It includes three offline English songs by Josh Woodward: Good to Go, On Brevity,
and Dizzy Spells. See [music attribution and licenses](THIRD_PARTY_NOTICES.md).

The example demonstrates:

- a Canvas-rendered ambient background, rotating record, and animated cover light sweep;
- a lyrics-focused desktop layout;
- a custom desktop title bar with native caption controls and drag behavior;
- functional For You, Discover, Library, and local search views;
- real play/pause, previous/next, automatic queue advance, track selection, seek, and volume;
- native duration and progress updates, loading and playback error feedback;
- local audio opened through the system file picker, including Android document URIs;
- illustrative demo lyrics that advance with the real playback position;
- an Android layout with swipeable Now Playing and Library pages;
- custom-styled sliders for playback progress and volume;
- a circular scene transition when switching tracks;
- a retained hover extension that follows the pointer with a 90px radial highlight.

Open local audio from the Library's more menu. Local tracks remain in the current
session. No storage permission or hardcoded device path is required. Supported
audio formats depend on the native player; MP3 and FLAC can be selected.
Lyrics marked DEMO are illustrative text, not the recordings' actual lyrics.
Account/profile actions remain UI previews.

## Build and run

Install the HuxerUI SDK and point `HUXERUI_HOME` to its root directory.

Windows:

```powershell
huxerui run windows --profile debug
```

Linux:

```bash
huxerui run linux --profile debug
```

macOS:

```bash
huxerui run macos --profile debug
```

Windows and Linux produce desktop executables. macOS produces an application bundle.

Android:

```powershell
huxerui run android --profile debug
```

Dependencies require network access for the first build. Playback of the three
bundled songs is offline. Dependency build directories are isolated under
`.huxerui` with shortened names for Windows toolchain path limits.
