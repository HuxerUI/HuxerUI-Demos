# Huxer Music

Huxer Music is a cross-platform HuxerUI interface example inspired by immersive desktop music players. It uses mock data and does not play audio.

The example demonstrates:

- a Canvas-rendered ambient background, rotating record, and animated cover light sweep;
- a lyrics-focused desktop layout;
- a custom desktop title bar with native caption controls and drag behavior;
- functional For You, Discover, Library, and local search views;
- mock play, favorite, previous, next, track-selection, seek, and volume interactions;
- custom-styled sliders for playback progress and volume;
- a circular scene transition when switching tracks;
- a retained hover extension that follows the pointer with a 90px radial highlight.

The primary play button transitions from `#1f2937` to `#273449` while its white spotlight fades in over 0.4 seconds. The spotlight is painted above the button background, below its icon, and clipped to the circular button shape.

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
