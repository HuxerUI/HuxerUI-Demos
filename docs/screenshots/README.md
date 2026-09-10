# Screenshot notes

These screenshots were captured from running demos on September 11, 2026. The desktop PNGs were supplied by the project maintainer and capture the Windows application windows without a mouse pointer. The Android images are full device screenshots. The gallery uses the original captures without mock device frames or composited UI.

| Image | Platform | Scene |
| --- | --- | --- |
| [Huxer Music](huxer-music-desktop.png) | Windows | For You, bundled "Good to Go" loaded and paused at 0:00, record artwork and illustrative DEMO lyrics |
| [Huxer Mail](huxer-mail-desktop.png) | Windows | Light theme, Inbox, no conversation selected |
| [Huxer Drive](huxer-drive-android.png) | Android | Light theme, Files → Recent, grid view with sample images and documents |
| [Huxer Drive preview](huxer-drive-preview-android.png) | Android | Light theme, "Morning light.png" image preview with Share, Download, and Details actions |

All screenshots use English interface text and sample content. Mail and Drive were captured with a temporary `en-US` locale override, then their source code and normal builds were restored. Screenshots show representative desktop and mobile layouts; see the [platform table](../../README.md#platforms) for included host projects.

## Refreshing the gallery

- Run the current app and reproduce the scene above with bundled demo content. Avoid personal messages, imported files, local music filenames, or open system dialogs.
- Use the English interface. If a temporary locale override is needed, restore the source and normal application build after capturing; do not commit the override.
- Keep the desktop window wide enough to show the full workspace. Use portrait orientation for Drive and wait for page transitions to settle before capture.
- Move the mouse pointer outside the captured window and check for cursor highlights or overlays before saving a desktop screenshot.
- Replace the corresponding image here and inspect it at the size used in the README. Keep filenames stable so the root and project READMEs share the same asset.
- Update the date and scene notes when captures change. If changing image formats, update every reference as well.

The root and Drive READMEs display the two Android images side by side at 320 pixels each, while retaining the full-resolution sources. Music attribution is recorded in [THIRD_PARTY_NOTICES.md](../../Huxer-Music/THIRD_PARTY_NOTICES.md).
