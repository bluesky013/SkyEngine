## Context

Builds on the archived `engine-ui-system` core, `ui-widgets-core`, and `ui-data`. Core already defines
`UIElement` (tree/layout/paint/events/properties), `UIPaintContext` (clip stack, batched textured quads),
`UIDrawData`/`UITextureId`, and the `IUIFontProvider` (glyph bitmap + metrics) and `IUITextureRegistry`
(register/update/release image) seams. `openspec/specs/ui-text` specifies the behavior; this change implements
it. `SKY_BUILD_FREETYPE` (plugins.json `cmake_var`, currently ON) gates the optional FreeType module, and the
existing FreeType plugin is bound to the legacy `render/text` stack, which the UI path must not depend on.

## Goals / Non-Goals

**Goals:**
- Built-in fallback provider; glyph atlas with page growth; measurement; draw-data emission; `Text` widget.
- An optional FreeType provider that is independent of the legacy render text stack.
- Deterministic, GPU-free unit tests, plus a real-font FreeType test when enabled.

**Non-Goals:**
- Auto word-wrap, shaping/bidi, SDF, DPI, and legacy `render/text` interoperability.

## Decisions

### 1. Provider seam usage and a built-in fallback
`UIBuiltinFontProvider` implements `IUIFontProvider` with procedural coverage glyphs and deterministic
advances (advance = round(0.6 * size), height = size, line height = size). It guarantees text works with no
plugins and makes measurement tests exact. Alternative: require a font file — rejected (breaks the fallback
requirement and headless tests).

### 2. Glyph atlas
`UIFontAtlas` caches `UIGlyphEntry` by `(size, codepoint)`, rasterizes on miss via the active provider, and
packs glyphs into fixed-size pages (row/shelf packer). Each page is uploaded once through
`IUITextureRegistry::RegisterTexture` (`UIImageData`, coverage expanded to RGBA8) and stored with its
`UITextureId`. When a page is full a new page is allocated; existing pages and handles are untouched, so
already-emitted draw data stays valid. Alternative: one texture per glyph — rejected (batch/asset churn).

### 3. Measurement and layout
`UITextLayout::Measure` splits on `'\n'`, returning width = max line advance sum and height = line count x
line height. `UITextLayout::Emit` walks glyphs, computing each quad from the glyph bearing and advancing the
pen; newline resets X and advances Y by line height. Emit writes through `UIPaintContext::AddTexturedQuad`,
so the current clip stack applies automatically. Alternative: a retained text object — rejected; immediate
emission matches the existing paint model.

### 4. Text system and widget
`UITextSystem` bundles an `IUIFontProvider`, an `IUITextureRegistry`, and a `UIFontAtlas`, and is injected into
widgets. `Text` (a `UIElement`) holds content/font size/color/alignment, measures through the system for AUTO
sizing, and paints through `UITextLayout::Emit`. It no-ops when no system is set, so layout stays safe before
text is configured. `Text` is registered in `UIElementRegistry` as the `Text` document type.

### 5. Optional FreeType provider
`FreeTypeUIFontProvider` (module `engine/ui/freetype`, target `UITextFreeType`, gated by `SKY_BUILD_FREETYPE`)
loads font bytes through `Core` `FileSystem::OpenFile` + `FilePtr::ReadBin`, creates an `FT_Memory_Face`, sets
pixel sizes, and returns `UIGlyphBitmap` from `FT_Load_Char(FT_LOAD_RENDER)`. It links `UI` + `Core` +
`3rdParty::freetype` and does not touch `render/text`. Alternative: adapt the existing `FreeTypeModule` —
rejected (it implements the legacy `TextRegistry` and would pull legacy render into scope).

### 6. Source layout and tests
Text lives under `engine/ui/core/{include/ui,src}/text/`. Tests are split into explicit targets in
`engine/test/ui/CMakeLists.txt`: `UICoreTest` (existing), `UITextTest`, and `UITextFreeTypeTest` (only when
`SKY_BUILD_FREETYPE`). Configure passes `-DSKY_BUILD_FREETYPE=ON` explicitly.

## Risks / Trade-offs

- **Procedural fallback glyphs are placeholders, not letterforms** -> acceptable as a fallback; real text uses
  FreeType.
- **Coverage expanded to RGBA8** -> simple and matches `UIImageData`; a single-channel image format is a
  later optimization.
- **Atlas pages never evict** -> fine for milestone; eviction/defrag is a follow-up.
- **FreeType test depends on `assets/fonts/OpenSans-Regular.ttf`** -> skipped when the plugin/module is off.

## Migration Plan

Additive: new text sources, one optional module, a test CMake restructure, and a registry entry. Rollback
removes the text/freetype sources and restores the previous test CMake. No render or Aurora changes.

## Open Questions

- Whether `UITextSystem` should live on `UIContext` rather than be injected per widget.
- When to add auto word-wrap and shaping (separate change).
- Whether atlas eviction/defrag is needed before shipping large font sets.
