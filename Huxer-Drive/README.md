# Huxer Drive

Huxer Drive is an offline personal cloud-drive demo with a blue-and-white workspace, original folder-and-cloud branding, and real app-owned file storage. It demonstrates organizing, importing, previewing, revising, sharing, and recovering personal files without an account or a server.

## Screenshot

<p align="center">
  <img src="../docs/screenshots/huxer-drive-android.png" width="320" alt="Huxer Drive on Android showing recent sample files in grid view and bottom navigation">
  <img src="../docs/screenshots/huxer-drive-preview-android.png" width="320" alt="Huxer Drive on Android previewing the sample Morning light image with Share, Download, and Details actions">
</p>

*Android · Recent files in grid view and image preview · Light theme*

[All demos](../README.md) · [Screenshot notes](../docs/screenshots/README.md)

## Try the core flow

- Open **Projects**, switch between list and grid, and browse the original sample files.
- Use **Upload** to import files or a directory where the platform picker supports it. Files are copied into the application data directory; the source stays untouched.
- Open a text file, choose **Edit text** (under the preview overflow menu on phones), save, and use **Versions** to restore or export an earlier version. On phones, open **Details** to reach versions and activity. Restoring creates another version and preserves history.
- Use a file's menu or selection checkboxes to rename, favorite, move, copy, or move files to the recycle bin. Long press selects on touch screens; the bottom bar then provides batch actions. Desktop files can also be dragged onto folders within the app.
- Open **My shares**, create a local share with a seven- or thirty-day expiry, and try the recipient view with an incorrect and then correct access code.
- Open **Transfers** to follow progress, inspect full filenames, and retry failed tasks. Open **Settings → Transfers** from the account menu to change the filename collision policy; **Settings → Demo information** contains the mock failure/retry demonstration and file limits.
- Restore items from the recycle bin, or permanently delete them after confirming. Storage includes file versions and items in the recycle bin, with shared content counted once.

Search matches names across the active area, ignoring ASCII letter case. Type filters and sorting refine results. Recent files are ordered by access or modification time. Click a filename or focus it and press Enter or Space to open it.

## Real operations and mock boundaries

Import/export uses the public HuxerUI `FilePicker`, `FileReference`, and `File` APIs. Import streams bytes with progress; file operations persist across application restarts. Directory import preserves successful files if a later file fails. Failed file transfers can be retried while their platform-granted source remains available. A failed directory enumeration requires selecting the directory again. Transfer history itself is session-only.

The asynchronous local store stands in for a remote service. Sharing is a persisted, deterministic **in-app simulation**: `drive-demo://share/...` is an inert demonstration identifier, not a public URL. Access codes are not authentication or encryption. The folder recipient demonstration returns to the personal workspace. There are no real users, team roles, remote storage, network uploads, background synchronization, resumable transfers, or system share integration.

Files up to 128 MiB can be imported. Text preview/editing is limited to 1 MiB; PNG/JPEG preview is limited to 24 MiB. Other formats remain downloadable. The 100 GiB capacity is a demo quota, not a promise of available disk space. The operating system or browser may enforce a smaller limit. Export uses the platform's save workflow; its public boolean result cannot distinguish cancellation from an I/O failure, so the transfer offers another save attempt.

## Architecture

| Source | Responsibility |
| --- | --- |
| `src/drive_model.*` | Pure C++ hierarchy, naming, selection, copy/move, trash, validation, and index serialization |
| `src/drive_store.*` | Application data storage, immutable content blobs, streamed import, versions, and recovery |
| `src/drive_actions.cpp` | Root-owned asynchronous tasks, transfers, file picker actions, and navigation |
| `src/drive_controls.cpp` | File artwork, width-aware single-line labels, search, and mobile action sheets |
| `src/drive_workspace.cpp` | Virtual file list/grid, search, filters, selection, drag/drop, and storage |
| `src/drive_preview.cpp` | Text/image preview, editor, versions, activity, and transfer views |
| `src/drive_settings.cpp` | Transfer preferences and demo information |
| `src/drive_dialogs.cpp` | Folder/name editing, destination picker, mock sharing, and purge confirmation |
| `src/drive_app.cpp`, `src/drive_theme.cpp` | Responsive shell, lifecycle, resources, semantics, and visual system |

The store writes checksummed indexes to two alternating generations and verifies each write before publishing its state. Startup chooses the newest valid index; it reports corruption instead of silently replacing an existing unreadable workspace with samples. Content blobs use numeric identifiers, so display names never become internal paths. Copying and version restoration reuse immutable blobs. Garbage collection after permanent deletion retains anything referenced by a recoverable index. This is a demo file store, not a transactional database or a backup service.

The UI uses only the current public SDK. It adds no platform bridge, database dependency, HTTP client, compatibility wrapper, or shared framework. File activation is consumed when the platform host provides granted files; no OS file associations are registered by this demo.

## Responsive layout and localization

- **Expanded:** labeled navigation, column-aligned list or adaptive grid, separate preview information panel, and a small transfer panel.
- **Medium:** compact navigation rail with the same file workspace; information remains beside the preview.
- **Compact:** the file home has search and All/Recent/Favorites tabs, with Files/Shares/Transfers navigation at the bottom. Folder pages replace that header with Back, the current folder and an ancestor picker, search, and a compact sort/view row. Selection replaces the header and bottom navigation with selection controls and batch actions. Preview uses the available content area, with Share/Download/Details at the bottom and details, versions, and activity in a sheet. Adding files, file actions, sorting, and account actions also use bottom sheets. List/grid content reserves scrollable space below its last item for the floating add button.

Peer pages use `Pager`: horizontal paging for compact bottom navigation and file tabs, and click-controlled vertical paging for desktop navigation. File tabs retain their position when returning from another main page. Folder and preview navigation use `NavigationStack`.

The desktop title bar has separate theme and settings buttons. Compact layouts expose these actions through the account menu. Light/dark appearance is session-only. Semantic labels, focus rings, keyboard actions, native menu/dialog focus, and a polite operation-status region are included. Platform Back first exits mobile selection or expanded search before leaving a folder. Bulk download exports selected non-folder files through sequential save pickers and stops if one is canceled or fails; multi-selection sharing asks which individual item to share and does not create a combined public link.

Application strings use Resources from the start: English by default (`en-US`) plus `zh-CN`, `zh-TW`, `ja-JP`, `ko-KR`, `fr-FR`, `de-DE`, `es-ES`, and `pt-BR`. The active platform locale selects the resource language. Sample document contents and the fictional owner are intentionally English.

## Build and run

Install the current HuxerUI SDK and the target platform toolchain, set `HUXERUI_HOME`, and run these commands from this directory:

```sh
huxerui doctor
huxerui build windows --profile debug
huxerui run windows
huxerui run android --device <device-id>
huxerui build web --profile debug
```

The CLI also accepts `linux`, `macos`, and `ios` with the corresponding toolchain. The six platform host projects live under `platform/`. Android needs its SDK/NDK and JDK; Web needs Emscripten; Apple targets require Xcode on macOS. The iOS host includes the generated Xcode scheme and resource staging scripts.

Serve the Web output directory reported by the CLI over localhost HTTP and open `huxer_drive.html`. Web uses the SDK browser-backed storage and picker implementation; storage belongs to the browser origin and is not shared with the native apps. Internal back navigation is provided in the app; browser URL history is not synchronized with folders.

Build outputs and local IDE state are ignored. No machine-specific SDK or toolchain path is required in the repository.

## Validation

The model scenarios cover subtree copy/move, destination cycles, duplicate selection, collisions, trash/share revocation, restoration, permanent deletion, immutable-content accounting, serialization, corrupt/truncated indexes, and path injection rejection. They can run independently of the SDK:

```sh
cmake -S tests -B .huxerui/model-tests
cmake --build .huxerui/model-tests
ctest --test-dir .huxerui/model-tests --output-on-failure
```

Windows, Android, and Web are the initial local validation targets. Linux, macOS, and iOS host projects are included; their presence does not imply that those platforms have been built or tested on this machine.
