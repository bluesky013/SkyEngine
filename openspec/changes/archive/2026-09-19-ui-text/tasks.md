## 1. Core text

- [x] 1.1 Add `text/UIBuiltinFontProvider.{h,cpp}`: procedural coverage glyphs with deterministic advances/metrics
- [x] 1.2 Add `text/UIFontAtlas.{h,cpp}`: `(size, codepoint)` cache, row packer, page registration via `IUITextureRegistry`, growth keeps old handles valid
- [x] 1.3 Add `text/UITextLayout.{h,cpp}`: `Measure` (explicit newlines) and `Emit` (quads referencing atlas handle, honoring current clip)
- [x] 1.4 Add `text/UITextSystem.{h,cpp}` bundling provider + registry + atlas

## 2. Text widget and registry

- [x] 2.1 Add `widgets/Text.{h,cpp}`: content/size/color/align, AUTO sizing via measurement, paint via text system, no-op when unset
- [x] 2.2 Register `Text` in `UIElementRegistry::CreateDefault()`

## 3. Optional FreeType module

- [x] 3.1 Add `engine/ui/freetype/` target `UITextFreeType` gated by `SKY_BUILD_FREETYPE`, linking `UI` + `Core` + `3rdParty::freetype`
- [x] 3.2 Implement `FreeTypeUIFontProvider` on FreeType directly (file load via core file system; `FT_New_Memory_Face`/`FT_Set_Pixel_Sizes`/`FT_Load_Char`), independent of `render/text`
- [x] 3.3 Register the optional subdirectory in `engine/ui/CMakeLists.txt` under the `SKY_BUILD_FREETYPE` gate

## 4. Tests and verification

- [x] 4.1 Restructure `engine/test/ui/CMakeLists.txt` into `UICoreTest` (existing sources + `main.cpp`) and `UITextTest` (`UITextTest.cpp` + `main.cpp`)
- [x] 4.2 Add `UITextTest.cpp`: fallback measure single/multi-line, atlas registration/reuse/growth, emit references atlas handle, clip respected, `Text` widget measure + paint, no-system no-op
- [x] 4.3 Add `UITextFreeTypeTest` (only when `SKY_BUILD_FREETYPE`): load `assets/fonts/OpenSans-Regular.ttf`, assert non-empty glyph bitmap and positive advance
- [x] 4.4 Configure with `-DSKY_BUILD_FREETYPE=ON`, build all test targets, and run them green (33 + 8 + 2 tests passing)
