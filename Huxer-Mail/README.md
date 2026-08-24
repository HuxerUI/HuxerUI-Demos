# Huxer Mail

Huxer Mail is a fully offline personal productivity mail client that uses a complete mail-handling workflow to exercise application-level HuxerUI capabilities. It never connects to a real mailbox and does not require an account, OAuth, IMAP, SMTP, or any network service.

## Product capabilities

- Inbox, Starred, Sent, Drafts, Outbox, and Trash folders, with Archive available through search.
- 56 deterministic mock threads with unread and starred states, conversations, attachments, and failed delivery.
- A `VirtualList` mail list with all, unread, and starred filters, delayed search, stable keys, and explicit batch selection.
- Conversation reading, expandable history, plain-text messages, quoted replies, image attachment previews, and attachment export.
- New messages and replies, email validation, empty-subject confirmation, real `FilePicker` attachments, session drafts, and deterministic sending.
- Archive, delete, read, unread, star, timed undo, and permanent deletion confirmation in Trash.
- An Outbox failure reason, editing, deterministic retry, and a polite live region.
- A deterministic first manual refresh failure, successful retry with a new system update, and direct success on later refreshes.
- Light and dark themes plus reduced motion, all scoped to the current session.

## Responsive structure

- Expanded: expanded navigation, mail list, and Reader or Composer in three columns.
- Medium: compact icon navigation with a two-column mail list and Reader; Composer or Settings replaces the main content region.
- Compact: `TopAppBar`, Drawer, and mail list; Reader, Composer, and Settings open as pages in a typed `NavigationStack`.
- Compact Web uses `BrowserNavigationStack` to synchronize `#/thread/...`, `#/compose`, and `#/settings` history.

## HuxerUI capabilities

The app directly uses the public SDK's Application, View DSL, Scope code generation, State, StateList, Environment, Theme, Viewport Class, VirtualList, NavigationPath, NavigationStack, BrowserNavigationStack, Drawer, Menu, Dialog, Toast, Tooltip, TextField, Validation, Task, Lifecycle, Semantics, Resources, Vector Image, Raw, File, FileSystem, FilePicker, Safe Area, System Bars, and Window APIs.

Views follow the HuxerUI DSL conventions: containers use braced children such as `Column{...}`, `Row{...}`, and `Stack{...}`; common view modifiers are composed with `.With(...)`; and components that own hooks use `[[huxerui::composable]]`. Application sources consistently use `using namespace huxerui;`.

The app does not use PlatformView, PlatformModule, ExternalTexture, HTTP, private headers, or internal HuxerUI platform implementations.

## Resources and data

- `resources/strings/default.properties` contains the `en-US` baseline and fallback for unsupported locales.
- Localized interface resources are included for `zh-CN`, `zh-TW`, `ja-JP`, `ko-KR`, `fr-FR`, `de-DE`, `es-ES`, and `pt-BR`.
- `resources/images` contains original linear SVG icons and an attachment preview illustration.
- `resources/raw` contains original mock attachments that can be read and exported offline.
- Mock messages are fictional user content written in English and do not change with the interface locale.

## Build and run

Install the HuxerUI SDK and point `HUXERUI_HOME` to its root directory.

```powershell
huxerui doctor all
huxerui build windows --profile debug
huxerui run windows --profile debug
```

Use `huxerui build <platform> --profile <profile>` with `windows`, `android`, `web`, `macos`, or `ios` and a `debug` or `release` profile. The repository contains the platform projects required by each target.

Web output is written to `.huxerui/build/web/<profile>` and must be served over HTTP with the correct JavaScript MIME for `.mjs` files and WebAssembly MIME for `.wasm` files. Use `huxerui devices android` to list Android targets and `huxerui open ios` to open the iOS project when platform tooling is available.

## Known boundaries

- State and drafts persist only for the current session.
- Messages render as plain text without HTML, MIME, remote images, or complex signatures.
- The app has no real accounts, background synchronization, push, or system notifications.
- The Web FilePicker's native browser chooser requires a real user gesture.
