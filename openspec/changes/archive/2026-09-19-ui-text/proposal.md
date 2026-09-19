## Why

`openspec/specs/ui-text` requires a text system (provider seam, glyph atlas, measurement, layout to draw data)
and the core `ui-core` capability requires a text widget, but neither exists. Text is a prerequisite for most
real screens and is render-agnostic at the core (it produces draw data and registers atlas pages through the
existing seams), so it can land while the Aurora pass waits. A FreeType-backed provider is also wired so real
fonts can be used without making FreeType a hard dependency of core.

## What Changes

- Add a built-in bitmap fallback `IUIFontProvider` so text works with no plugins.
- Add a glyph atlas that caches `(size, codepoint)`, packs glyphs into pages, and registers each page through
  `IUITextureRegistry` (creating new pages when full while keeping existing handles valid).
- Add text measurement (explicit newlines; width = longest line advance sum, height = lines x line height) and
  layout that emits render-agnostic quads referencing the atlas handle and honoring the current clip.
- Add a `UITextSystem` (provider + texture registry + atlas) and a `Text` widget (content/size/color/align,
  AUTO sizing from measurement); register the `Text` document type.
- Add an optional FreeType-backed provider module (`engine/ui/freetype`, target `UITextFreeType`) gated by
  `SKY_BUILD_FREETYPE`, implemented directly on FreeType and independent of the legacy `render/text` stack.
- Tests: `UITextTest` (fallback/layout/atlas/widget) and `UITextFreeTypeTest` (real font via
  `assets/fonts/OpenSans-Regular.ttf`), the latter only when `SKY_BUILD_FREETYPE` is enabled.

**Non-goals**: auto word-wrap, non-Latin shaping/bi-directional text, SDF atlases, DPI-aware rasterization,
and reusing the legacy `render/text` `Font`/`TextRegistry` types.

## Capabilities

### New Capabilities
<!-- None: implements and refines the existing `ui-text` capability. -->

### Modified Capabilities
- `ui-text`: add concrete requirements for a built-in fallback provider, the text widget, and an optional
  FreeType provider module.

## Impact

- New `engine/ui/core/include/ui/text/` + `src/text/` and `widgets/Text.{h,cpp}`; `Text` registered in
  `UIElementRegistry`. `engine/ui/freetype/` optional target gated by `SKY_BUILD_FREETYPE`.
- New tests and an `engine/test/ui/CMakeLists.txt` restructure into `UICoreTest` + `UITextTest`
  (+ `UITextFreeTypeTest`).
- No `UIRender`/Aurora changes; core stays render-agnostic.
