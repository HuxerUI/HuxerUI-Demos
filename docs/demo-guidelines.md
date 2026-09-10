# Demo development guidelines

These principles guide application-level demos in this repository. For the current apps and screenshots, see the [showcase](../README.md).

## Repository scope

Applications in HuxerUI-Demos should:

- Complete a meaningful user task instead of presenting static screens or component galleries.
- Show how state, navigation, responsive layout, asynchronous work, input, resources, lifecycle, and accessibility form a cohesive product.
- Use information architectures designed for desktop and mobile rather than scaling one layout down.
- Use original product names and visual systems. Prefer original graphics and content; include attribution and license notices for any third-party assets, such as bundled music.
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
- Plan app interface localization through HuxerUI Resources for `en-US`, `zh-CN`, `zh-TW`, `ja-JP`, `ko-KR`, `fr-FR`, `de-DE`, `es-ES`, and `pt-BR`.

Localization is a development target, not a statement of coverage in every app. See each project README for its current implementation.

See the [demo design guide](demo-planning.md) for capability planning and application architecture.
