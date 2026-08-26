# HuxerUI-Demos

HuxerUI-Demos hosts complete application-level demos built with HuxerUI.

These demos do not isolate individual controls or APIs. Each app has a distinct product identity, a complete user flow, and a cross-platform information architecture that tests HuxerUI as an application framework.

## Repository scope

Applications in HuxerUI-Demos should:

- Complete a meaningful user task instead of presenting static screens or component galleries.
- Show how state, navigation, responsive layout, asynchronous work, input, resources, lifecycle, and accessibility form a cohesive product.
- Use information architectures designed for desktop and mobile rather than scaling one layout down.
- Use original product names, visual systems, graphics, and content without commercial branding or copyrighted assets.
- Run offline; any network-oriented behavior must have a deterministic local implementation or fallback.
- Use the HuxerUI resource system directly for localization.
- Keep an independent product identity and visual language for every demo.

## How demos differ from examples

An example is best suited to answer, "How do I use this API?" A demo in this repository should answer, "Can these capabilities form a credible, interactive, cross-platform application?"

In addition to primary screens, application-level demos should cover the loading, empty, error, retry, editing, and success states required by their core flows. They should also account for keyboard, pointer, touch, focus, and accessibility behavior.

## Planning principles

- Every demo must be complete without depending on an uncontrolled backend or media stack.
- Each new demo should validate additional areas of the framework instead of repeating the same list-and-detail pattern.
- Do not build a large shared demo framework in anticipation of reuse.
- Extract shared visual assets, mock infrastructure, or base components only after at least two demos contain genuine duplication.
- Mock behavior must be deterministic and repeatable while representing realistic loading, failure, retry, and success states.
- Persistence is a product decision for each demo and should follow its core user flow.
- Repository documentation, source comments, identifiers, and default presentation content use English.
- App interface text is localized through HuxerUI Resources for `en-US`, `zh-CN`, `zh-TW`, `ja-JP`, `ko-KR`, `fr-FR`, `de-DE`, `es-ES`, and `pt-BR`.

## Demos

[Huxer Mail](Huxer-Mail/README.md) is an offline-first personal productivity mail client. It implements mail browsing, search, conversation reading, composing and replying, attachments, drafts, archive and delete undo, Outbox retry, deterministic synchronization, responsive navigation, localization, themes, and accessibility semantics.

Huxer Mail is self-contained under `Huxer-Mail` and includes Windows, Linux, Android, Web, macOS, and iOS platform projects. Every demo keeps an independent product and project boundary.

See the [Demo design guide](docs/demo-planning.md) for the public SDK capability assessment, candidate coverage, and Huxer Mail architecture.
