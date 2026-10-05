## Why

Editor UI needs iconography, but there is no icon pipeline: bitmap assets are per-DPI and
per-resolution, and rasterizing SVG at runtime is expensive and repeated every launch. The engine
also lacks a generic place to store expensive, reproducible derived artifacts (first-launch bake,
cached for later runs, invalidated by input changes) — icons are only the first consumer.

## What Changes

- Add a generic, content-addressed **Derived Data Cache (DDC)** to `engine/framework`: builders
  registered by id produce derived bytes from source bytes; results are cached on disk under a key
  derived from source + builder id + builder version + settings + platform, so any input change
  invalidates the entry.
- Add a UI-icon builder (`ui-icon-svg`) implemented in the sandbox module using **NanoSVG** to
  rasterize SVG icons into RGBA8, registered with the DDC during module init. NanoSVG is referenced
  only by the sandbox module (no engine-wide dependency).
- Add a save icon resource (`engine/sandbox/resources/icons/save.svg`) and a demo in the reflection
  panel that bakes it through the DDC and draws it as an icon button, storing derived output under
  the sandbox resources directory (`<resources>/cache/`). When the source SVG is unavailable the
  demo falls back to a directly generated glyph.
- Add NanoSVG as a managed, header-only third-party (`cmake/thirdparty.json`,
  `cmake/thirdparty/Findnanosvg.cmake`) for the supported desktop platforms.
- Add framework tests covering DDC miss/hit, settings/platform keying, and builder-version
  invalidation.

This change does not introduce a runtime texture atlas or block compression, nor is it a general
icon theme/toolbar rollout; those are follow-ups.

## Capabilities

### New Capabilities

- `derived-data-cache`: generic, content-addressed derived-data cache with registered builders.
- `editor-ui-icons`: SVG icon builder that produces cached RGBA icons through the DDC.

### Modified Capabilities

<!-- none -->

## Impact

- `engine/framework`: new `framework/asset/DerivedDataCache.{h,cpp}` (generic, editor-independent).
- `engine/sandbox/module`: new `UiIconBuilder` (builder + `InstallUiIconBuilder`) registered from
  `SandboxModule::Init`, which also points the DDC root at `<sandbox resources>/cache`.
- `engine/sandbox/shell`: the reflection demo panel bakes/draws the save icon sample.
- `engine/sandbox/resources/icons/save.svg`: the sample icon source.
- `engine/test/framework`: new `DerivedDataCacheTest`.
- New third-party: `nanosvg` (header-only, desktop platforms only).
- No engine-wide third-party exposure; NanoSVG is confined to the sandbox module.
