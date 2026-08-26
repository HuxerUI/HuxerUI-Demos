# HuxerUI-Demos Design Guide

This document summarizes the public HuxerUI capability model, application-level demo requirements, candidate coverage, and the product and technical definition of Huxer Mail.

## Assessment basis

Capability claims use only the public SDK surface:

- HuxerUI CLI commands.
- Public SDK headers.
- Public HuxerUI CMake package, resource tools, and code generators.
- Public platform artifacts distributed with the SDK.

Demos do not read or depend on HuxerUI source files under `src`, private headers, or internal platform implementations. Missing documentation is never used to infer API behavior.

## HuxerUI capability model

### Application and runtime

HuxerUI applications are written in C++20. `Application` receives a root View factory and application options, while the platform Runtime handles window metrics, input, activation, lifecycle, frame construction, rendering submission, and accessibility submission.

Applications can receive URL and file activation events at launch and during execution. They can observe Active, Inactive, and Background lifecycle states. Public Window APIs expose safe areas, edge-to-edge presentation, system or custom title bars, system-bar appearance, and desktop window controls.

### Declarative Views and state

The View DSL describes interfaces through View factories, `Scope`, scope code generation, and composed layouts. Views support stable keys, and the Runtime reconciles the mounted tree.

`UseState` and `StateList` keep data in composition scope. Reading state during composition subscribes the active scope, and state changes recompose relevant scopes. StateList supports insertion, updates, movement, and deletion.

Lifecycle hooks establish effects on mount and clean them up when dependencies change or a component unmounts. TaskScope associates HuxerUI coroutine tasks with component lifecycle and supports cancellation and delay.

### Environment and Theme

Environment is a typed, hierarchical channel for configuration and services. An app can provide custom Environment Values or Root Services and consume them in descendant scopes.

Theme builds on Environment and exposes colors, typography, shapes, spacing, shadows, motion, interaction feedback, and common control styles. The SDK provides light and dark Flat and Material definitions and allows apps to override a complete theme or an individual style.

### Layout and responsive structure

Core layouts include Column, Row, Flow, Stack, ScrollView, VirtualList, and VirtualGrid. VirtualList and VirtualGrid provide real virtualization, cache ranges, scroll positioning, and collection semantics.

HuxerUI supplies Compact, Medium, and Expanded Viewport Classes as responsive inputs. Applications explicitly choose an information architecture for each class instead of expecting the framework to rearrange a product automatically.

Public layout interfaces also permit custom Layout and Modifier implementations. Demos should prefer existing public capabilities and add extensions only when a concrete product requirement cannot otherwise be expressed.

### Navigation and presentation

Navigation includes a basic NavigationStack and a typed route stack driven by `NavigationPath<Route>`, with Push, Pop, Replace, and SetPath operations. Web additionally exposes routing that synchronizes browser history and URLs.

Product navigation components include TopAppBar, NavigationBar, compact and expanded NavigationPane variants, and a Drawer that can become modal when space is constrained.

Presentation APIs include Toast, Dialog, BottomSheet, Popup, Menu, and Tooltip. They use independent layers with focus trapping, scrims, outside-click behavior, cancellation policies, and semantic modal scopes.

### Animation

Public animation specifications include Snap, Tween, Spring, Keyframe, delay, repetition, and reverse playback. Views can declaratively animate opacity, translation, scale, and rotation and can compose Transitions.

Navigation and presentation have dedicated motion styles. Scene transitions support Fade and Circular Reveal and can degrade according to the Theme's reduced-motion setting.

### Resources, images, and drawing

At build time, the resource system compiles resource roots into namespaced packages and can merge framework, application, and HuxerUI Library resources. Runtime resource configuration includes locale and display scale.

Public resource types include localized strings, Raw data, PNG/JPEG images, and HuxerUI vector resources. Image supports scale modes, alignment, sampling, and tinting. Canvas and PaintContext can draw geometry, gradients, text, images, paths, shadows, clips, and transforms.

Repository documentation, source comments, identifiers, and default presentation content use English. App interface strings are supplied through Resources for `en-US`, `zh-CN`, `zh-TW`, `ja-JP`, `ko-KR`, `fr-FR`, `de-DE`, `es-ES`, and `pt-BR`. User-generated content does not need to change with the interface locale.

### Text input and validation

TextField supports single-line and multiline input, maximum length, secure entry, input types, submit actions, labels, placeholders, leading and trailing icons, and validation states.

The underlying input model includes IME composition, selection, caret and range geometry, clipboard operations, and editing actions. Public Semantics can express selection, read-only, required, and error states.

### Files, HTTP, and asynchronous work

File exposes synchronous and asynchronous reading, writing, appending, directory creation, enumeration, copying, moving, and deletion. FileSystem provides application data, cache, temporary, and current directories. FilePicker opens one or more files and saves files.

The current public FilePicker does not select directories. The public API also does not expose file watching, system thumbnails, or system-level drag and drop.

HTTP exposes common methods, headers, string request bodies, timeouts, responses, and errors. It suits ordinary request/response mocks or backend calls. The current public API does not include streaming requests, download progress, WebSocket, or real-time push.

Web uses IDBFS for persistent app-data and cache directories. Every demo must still provide deterministic offline data and fallback behavior.

### Lifecycle and accessibility

Component Lifecycle and TaskScope support subscriptions, asynchronous requests, and cleanup. Application lifecycle can pause simulated activity, refresh after resume, or respond to reactivation.

Semantics exposes roles for text, headings, images, buttons, links, form controls, tabs, menus, dialogs, navigation, lists, grids, and scrolling containers. It also supports state, range values, collections, live regions, custom actions, and platform-View association.

Accessibility is part of page and interaction design. Each demo should define semantic structure, focus order, keyboard behavior, and reduced-motion behavior before visual work is considered complete.

### PlatformView, PlatformModule, and ExternalTexture

PlatformView places a native View in the HuxerUI layout tree, sends properties through PlatformPayload, and receives typed events. PlatformModule opens native module instances and supports asynchronous calls, cancellation, event subscriptions, and cleanup.

Windows, Android, macOS, iOS, and Web all expose corresponding PlatformView factory types. An application must still register a concrete native implementation for each platform; these factories do not imply that a particular native control already exists.

ExternalTexture is a display channel for externally produced frames: Windows and Linux accept pixel frames, Android accepts Bitmap, Apple platforms accept CVPixelBuffer, and Web accepts browser video frames. It does not provide decoding, audio output, audio/video synchronization, buffering, or playback controls.

## Project and platform model

The HuxerUI CLI creates apps and libraries, adds platforms, diagnoses environments, lists devices, builds, runs, packages, and opens iOS projects.

The CMake package provides:

- `huxerui_add_app` for application targets.
- `huxerui_add_library` for HuxerUI static libraries.
- `huxerui_use_library` for local libraries or HTTPS libraries pinned to a complete commit SHA.
- Scope source generation.
- Resource compilation, package merging, and platform resource distribution.

The public CMake contracts describe these artifacts:

- Windows and Linux produce desktop executables.
- macOS produces an application bundle.
- Android builds the C++ application target as a shared library integrated by an Android host and AAR.
- iOS links the application core as a static archive into an iOS host project.
- Web builds an Emscripten ES module and preloads the resource package.

## Capabilities suitable for direct demonstration

The SDK is already suitable for data-driven, offline-first clients with multiple screens and meaningful interaction, including communities, mail, commerce, task or knowledge tools, and app-sandbox file workspaces.

Demos can directly show:

- State-driven mutation and localized recomposition.
- Distinct desktop and mobile navigation and master-detail structures.
- Typed navigation, Drawer, Dialog, Menu, BottomSheet, and Toast.
- Virtualized lists and grids, search, filters, and scroll positioning.
- Localized resources, original images, vectors, and custom Canvas drawing.
- Input, validation, clipboard, attachments, and file selection.
- Deterministic loading, failure, retry, and offline fallback.
- Lifecycle pause and resume, URL activation, and file activation.
- Reduced motion, keyboard focus, and Semantics.
- Custom window chrome and the extension boundaries of PlatformView and ExternalTexture.

## Capabilities that remain unconfirmed or require platform work

The following should not be presented as built-in HuxerUI capabilities:

- Audio playback, media decoding, and audio/video synchronization.
- WebSocket, real-time push, and system notifications.
- Databases, full-text indexing, and background synchronization.
- Camera, recording, system sharing, and permission management.
- System drag and drop, directory selection, and file watching.
- HTML mail, general Markdown, and rich-text editing.
- Native payment, account authentication, and system media controls.

External libraries or PlatformModule and PlatformView can extend these areas, but each extension introduces explicit platform engineering and cannot be inferred from existing public APIs.

## Application-level demo requirements

An ordinary example isolates an API. An application-level demo should provide:

- A core user task that can be completed.
- Loading, empty, error, retry, and success states related to that task.
- Search, filter, favorite, draft, or editing state that changes for real.
- Independently designed desktop and mobile information architectures.
- Pointer, keyboard, touch, focus, and accessibility behavior.
- Original branding, product content, and visual assets.
- A deterministic data source that runs offline.

An application should not be a set of static screenshots or a component gallery disguised as a product.

## Candidate demo analysis

### Music player

The core flow covers browsing recommendations and a library, opening an album, managing a queue, favoriting, and controlling Now Playing. Screens could include Home, Search, Library, Album, Queue, and a full-screen player.

It would demonstrate image resources, VirtualList, Slider, Canvas waveforms, shared playback state, scene transitions, responsive navigation, and lifecycle. Mock data would require original cover art, track metadata, simulated progress, and deterministic queue behavior.

Windows could use persistent navigation and a bottom playback bar, while mobile could use bottom navigation, a mini player, and a full-screen player. The SDK has no public audio playback API, so real playback requires decoding, audio output, and per-platform integration. This is visually strong but better reserved for a media-focused later demo.

### Forum community

The core flow covers browsing categories and topics, searching and filtering, reading, replying, posting, favoriting, and reviewing personal contributions. Primary areas would include Home, Categories, Saved, Profile, Topic, and Editor.

It would demonstrate VirtualList, typed navigation, tabs, chips, TextField, validation, Task, Toast, Dialog, Menu, resources, and responsive master-detail structure. Original avatars, illustrations, posts, and replies could support deterministic loading, paging, publishing, failure, retry, and offline fallback without a backend.

Windows could present navigation, topics, and details together; mobile could use bottom navigation and a push stack. The direction has strong framework value but overlaps mail and messaging in list, detail, and input coverage.

### Instant messaging

The core flow covers selecting a conversation, reading history, sending, retrying failure, searching, and viewing conversation details. Screens include Conversations, Chat, New Conversation or Contacts, Details, and Settings.

It would validate frequent state updates, message scrolling, IME behavior, Task, lifecycle, attachments, live regions, and desktop split layout. Mock behavior could simulate delayed delivery, typing, read state, and failure.

Mobile would navigate from the conversation list to chat, while desktop would keep both visible. The SDK has no WebSocket, push, system notification, voice, or video API. Reverse timelines, loading older messages, and scroll anchoring also require focused validation. It is a later candidate because its coverage overlaps mail and forums.

### Mail client

The core flow covers folders, search, reading, replying, composing, session drafts, archive, read state, star, delete, attachments, simulated sending, and deterministic synchronization.

Primary areas are folder navigation, mail list and filters, Reader, Composer, search, and Settings. It demonstrates NavigationPane, Drawer, master-detail layout, VirtualList, Menu, Dialog, Toast, multiline TextField, email validation, FilePicker, Task, lifecycle, Semantics, Theme, and resource localization.

Expanded uses navigation, list, and Reader or Composer in three columns. Medium uses compact navigation and two content columns. Compact uses a Drawer and a page stack. Plain-text messages avoid unsupported HTML and MIME requirements.

This direction matches the productivity focus and provides broad framework coverage without depending on media or a real backend. Huxer Mail implements this coverage as a self-contained demo.

### Commerce storefront

The core flow covers product browsing, search and filters, product detail, cart changes, checkout information, and local order confirmation.

It would demonstrate VirtualGrid, image resources, responsive merchandising, BottomSheet, chips, animation, form validation, and cross-page cart state. Original product imagery, catalog data, inventory, prices, and deterministic checkout would avoid real payment and backend dependencies.

It is visually strong but does not match the current productivity direction and provides less file, lifecycle, and platform-extension coverage. A later inventory or order-workbench interpretation may be more suitable.

### File manager

The core flow covers browsing directories, switching list and grid modes, previewing, creating folders, renaming, copying, moving, confirming deletion, and importing or exporting files.

It would demonstrate File, FileSystem, FilePicker, Task, VirtualList, VirtualGrid, Menu, Dialog, Window, image previews, and file activation. Windows could use a tree, toolbar, content area, and details panel; mobile could use single-level navigation and action menus; Web would explicitly represent an app-owned IDBFS workspace.

Because public FilePicker cannot select directories and the SDK has no file watching, system thumbnails, system drag and drop, or cross-platform arbitrary-directory abstraction, this demo must be an application sandbox workspace rather than a system file explorer. It overlaps little with other candidates and is a strong later validation target.

### Short-video player

The core flow covers a video feed, play and pause, seeking, switching videos, comments, and creator pages. It could theoretically exercise ExternalTexture, VirtualList, lifecycle, transitions, PlatformView, and overlay UI.

ExternalTexture only displays frames. It does not decode media or provide audio, synchronization, buffering, or playback control, and every platform requires a distinct frame source and media pipeline. This direction should wait until a separate media architecture is confirmed.

### Offline project or knowledge workspace

The core flow covers creating and switching projects, browsing tasks and notes, editing titles and content, maintaining status and tags, searching and filtering, attaching files, and reviewing activity.

It would demonstrate master-detail layout, StateList, input and validation, file attachments, Task, presentation, responsive navigation, Semantics, and a custom theme without a backend. Windows could show navigation, list, and editor together, while mobile could use a page stack.

This is technically low risk and aligns with productivity, but it needs a strong brand and information design to avoid becoming a generic administration dashboard.

## Demo product constraints

- Every demo targets both framework-validation depth and visual finish.
- Every target platform should have a deliverable project form.
- Deterministic mock network, sending, checkout, or playback behavior is acceptable.
- Localization uses HuxerUI Resources from the beginning of each project.
- Repository-facing content and the default app presentation use English.
- App UI supports nine regions: `en-US`, `zh-CN`, `zh-TW`, `ja-JP`, `ko-KR`, `fr-FR`, `de-DE`, `es-ES`, and `pt-BR`.
- Persistence follows the product's core flow rather than serving as a framework checklist item.
- Each demo has its own product identity and visual language.
- New demos should extend capability coverage rather than duplicate Huxer Mail's list-and-detail workflow.

## Huxer Mail product definition

### Positioning

Huxer Mail is a mail-processing workspace for individual professionals. It emphasizes efficient reading, filtering, replying, and archiving for project communication, reviews, file confirmation, and schedule coordination.

It does not center account administration, mail protocols, team assignment, or marketing mail. All contacts, organizations, attachments, and messages are original fictional content.

### Core flow

The app supports:

- Browsing Inbox, Starred, Sent, Drafts, Outbox, and Trash.
- Filtering all, unread, and starred mail.
- Searching sender, subject, and message content.
- Reading conversations, expanding history, and replying.
- Updating read state, star, archive, delete, and undo.
- Composing, validating recipients, attaching files, and simulating send.
- Keeping drafts for the current session.
- Retrying a deterministic Outbox failure.
- Manually refreshing through failure and successful retry.
- Switching light and dark themes and reduced motion for the current session.

### Page boundaries

The shell owns folder navigation, compose access, unread count, synchronization state, and Settings. It excludes multiple accounts, account menus, and user-created folders.

The mail list owns its title, search, filter menu, refresh, explicit selection mode, and VirtualList. A row shows an initial avatar, sender, subject, excerpt, time, unread state, star, attachment state, and conversation count.

The Reader owns the subject, actions, participants, time, attachments, latest body, collapsible history, and reply access. Messages use plain-text typography and a simple quoted region without HTML, remote images, or complex signatures.

The Composer owns recipients, subject, multiline body, attachments, send, and discard. Recipients are validated as email addresses; an empty subject requires confirmation; and closing unfinished content asks whether to keep or discard a session draft.

Search updates the current list instead of opening an advanced-search page. Settings contains only theme, reduced motion, and the offline-demo explanation.

### Responsive information architecture

Expanded presents expanded navigation, mail list, and Reader or Composer in three columns. The Reader receives the most space and constrains the maximum text width.

Medium presents compact navigation with the mail list and Reader in two columns. Composer replaces the main content region.

Compact presents a TopAppBar, mail list, and folder Drawer. Reader and Composer are pages in a NavigationStack, with secondary actions in a Menu.

These layouts change navigation behavior rather than merely hiding elements in one scaled layout.

### Visual direction

The visual language is spacious, fresh, restrained, and suitable for a high-information productivity app.

- A porcelain-white light foundation uses soft cool-gray surfaces and a gemstone-blue accent.
- The dark theme uses deep blue-gray surfaces and a lifted blue accent rather than pure black.
- Large structural regions remain open while mail rows stay compact.
- Background luminance, spacing, typography, and light dividers establish hierarchy.
- Corners are moderate, and zero-offset shadows are reserved mainly for floating layers.
- The current message uses a subtle blue surface; unread state also uses weight and semantics.
- Initial avatars and icons use low-saturation geometric colors and original linear vectors.
- Motion is short and restrained, with large movement removed under reduced motion.

### Localization

All interface strings, validation, Toasts, Dialogs, states, time and count templates, and accessibility labels use HuxerUI Resources. `default.properties` is the `en-US` baseline and unsupported-locale fallback. Additional locale files cover `zh-CN`, `zh-TW`, `ja-JP`, `ko-KR`, `fr-FR`, `de-DE`, `es-ES`, and `pt-BR`.

Mock mail is English user content and does not change with the interface locale. Locale follows platform ResourceConfiguration. No in-app language switcher is included because the current public API does not confirm app-controlled platform locale changes.

### Explicit exclusions

- Real accounts, OAuth, IMAP, and SMTP.
- HTML mail, MIME, remote images, signatures, and encryption.
- Contact management and address autocomplete.
- Multiple accounts, unified inbox, custom folders, and rules.
- Cross-restart draft or mail-state persistence.
- Background synchronization, system notifications, and badges.
- Calendar invitations and meeting systems.
- An advanced keyboard-shortcut system.
- File drag and drop, directory attachments, and complex attachment parsing.

## Huxer Mail mock design

### Initial data

The app creates 56 deterministic threads without random values:

- About 30 Inbox threads, including about eight unread.
- Eight Sent threads.
- Three Drafts.
- One failed Outbox thread.
- Four Trash threads.
- About ten Archive threads, available through search rather than primary navigation.
- Starred is cross-folder state and does not duplicate threads.

Most threads contain one message, about ten contain two to four messages, and about eight include attachments. Times use fixed offsets from app launch so content feels recent. Subjects cover releases, design reviews, research, project reporting, approvals, meetings, travel, partnerships, purchasing, and automation updates.

### Data concepts

Thread stores a stable ID, subject, participants, messages, star state, latest activity, and folder state. Message stores sender, recipient, time, plain-text body, direction, and attachments. Draft stores an optional related thread, recipient, subject, body, attachments, and edit time.

Attachments are either packaged mock resources or session files selected with FilePicker. Images can be previewed, while PDF and text files show metadata without parsing.

### Deterministic asynchronous rules

- Local mail appears immediately at startup, followed by a fixed-delay successful background synchronization.
- The first manual refresh fails; retry succeeds and adds a system update; later refreshes succeed.
- Search waits about 150 to 200 milliseconds and cancels the previous task when input changes.
- New messages and replies send successfully.
- Retrying the preconfigured Outbox failure succeeds.
- FilePicker cancellation is normal; attachment read errors affect only their row.
- Archive and delete update optimistically and offer a timed Toast undo.
- Star, read, and unread updates are immediate.

Background state pauses nonessential motion and refresh presentation. Returning Active updates relative times and performs a lightweight check. Sending is an application-level task and does not disappear when Composer closes.

## Huxer Mail technical design

### Project boundary

Huxer Mail is an independent HuxerUI project at repository root under `Huxer-Mail`, with Windows, Linux, Android, Web, macOS, and iOS platform directories compatible with the HuxerUI CLI project model.

Sources remain flat and split only by real responsibilities such as entry point, model, mailbox state, mock service, Views, and Theme. There is no shared demo library or general demo framework.

### State model

Mailbox data, navigation, interaction, and application state are separated. Threads and drafts use StateList. Folder, search, selection, and typed NavigationPath use independent State. Synchronization, sending, theme, and lifecycle are application-level state.

The root Scope creates state handles and provides them through a lightweight Environment Context. A Root Hook registers the mock service. The design avoids both one monolithic `State<AppState>` and excessive field-level fragmentation.

### Views and navigation

Primary recomposition boundaries include App Root, Mailbox Scene, Folder Navigation, Mail List, Mail Row, Reader, Composer, and Settings. Rows use stable thread IDs as keys, and the mail list uses VirtualList.

A typed `NavigationPath<MailRoute>` controls Reader, Composer, and Settings. Folder and search state do not enter browser history. Compact uses page Push and Pop; Medium and Expanded derive split scenes from the same route; Web uses BrowserNavigationStack where public behavior permits.

### Tasks and mock service

The mock service returns deterministic HuxerUI Tasks and does not own UI state. Search belongs to Mail List scope, manual synchronization belongs to Mailbox Scene, send and Outbox retry belong to App Root, attachment selection belongs to Composer, and Toast undo belongs to Mailbox Scene.

The app does not call HTTP merely to demonstrate an API that the product does not need.

### Files and attachments

User attachments come from FilePicker FileReferences and retain name, size, and type for the session. Packaged incoming attachments use Raw or Image resources. Export writes a mock attachment to the app temporary directory before using SaveFile. Images can use ImageAsset previews; other types show metadata only.

### Theme, presentation, and semantics

The app provides an independent ThemeDefinition over ThemeSpec, with light and dark palettes, control styles, and MotionScheme.

Toast provides feedback and undo; Dialog confirms permanent deletion, draft discard, and empty subjects; Menu contains secondary actions; Tooltip labels compact desktop controls; and Drawer provides Compact folder navigation. BottomSheet and Popup are not added without a product requirement.

VirtualList exposes the mail collection. Rows communicate unread, star, attachment, and failure states through both text and Semantics. Synchronization and sending use polite live regions, icon buttons have localized labels, and Dialog and Drawer preserve focus boundaries.

### Platform scope

Desktop uses public custom Window chrome; mobile uses the system window without the desktop title bar. All platforms use Safe Area and appropriate system-bar appearance. Huxer Mail does not use PlatformView, PlatformModule, or ExternalTexture.

The shared application layer uses only public HuxerUI APIs and contains no unconditional implementation for a different platform. See the [Huxer Mail README](../Huxer-Mail/README.md) for product capabilities and build instructions.
