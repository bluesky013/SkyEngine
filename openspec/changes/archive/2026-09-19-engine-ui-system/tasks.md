## 1. Module scaffolding and build

- [x] 1.1 Create `engine/ui/core/include/ui/`, `engine/ui/core/src/`, `engine/ui/render/include/ui/`, `engine/ui/render/src/`, and `engine/test/ui/` (tests follow the repo's central `engine/test/<module>` aggregator instead of `engine/ui/test/`)
- [x] 1.2 Add `engine/ui/CMakeLists.txt` with `add_subdirectory(core)` and `add_subdirectory(render)`; register `add_subdirectory(ui)` in `engine/CMakeLists.txt`
- [x] 1.3 Add `engine/ui/core/CMakeLists.txt` defining static target `UI` linking `Core` and `Framework`
- [x] 1.4 Add `engine/ui/render/CMakeLists.txt` defining static target `UIRender` linking `UI`, `Aurora.Pipeline`, `Aurora.RHI`, `Aurora.Shader`
- [x] 1.5 Add a configure-time guard in `engine/ui/core/CMakeLists.txt` that fails if any public UI header includes `aurora/` or `render/`

## 2. UI core: element tree and context

- [x] 2.1 Implement `UIElement` base (id, parent/child links, add/remove child, visibility, enabled, bounds)
- [x] 2.2 Implement `UIContext` owning the single root element and child traversal in paint order
- [x] 2.3 Implement dirty flags for layout and paint with subtree propagation and clean-subtree skipping
- [x] 2.4 Add `UICoreTest` target with tests for add/remove child, single root, visibility, and dirty propagation

## 3. UI core: layout and style

- [x] 3.1 Implement measure/arrange two-pass layout with box size rules and anchor rules (`UILayout.h`, `UIElement::Measure/Layout`)
- [x] 3.2 Implement resolved-bounds computation and parent content-box shrinking (`ComputeElementBounds`, `UILayoutParams::ContentRect`)
- [x] 3.3 Define `UIStyle`/`UITheme` value structs, style classes, and runtime theme replacement (`UIStyle.h`, `UITheme::Resolve`)
- [x] 3.4 Add `UICoreTest` cases for fixed-size arrange, parent shrink, and theme-change affecting resolved style

## 4. UI core: draw data contract and resource seam

- [x] 4.1 Define `UIVertex { pos, uv, color }`, index, and `UIDrawCmd { indexOffset, indexCount, clip, textureId }` (`UIDrawData.h`)
- [x] 4.2 Define `UIDrawData` and a `UIPaintContext` that batches primitives into it (rect, textured quad)
- [x] 4.3 Define `IUITextureRegistry`, `IUIFontProvider`, and `IUIAssetResolver` core-side seams
- [x] 4.4 Add `UICoreTest` cases for clip intersection in draw commands; GPU-type absence enforced by the render-agnostic header guard

## 5. UI core: input, events, focus

- [x] 5.1 Define `UIInputState` and `UIPointerEvent` / `UIKeyEvent` / `UITextInputEvent` types (`UIEvent.h`)
- [x] 5.2 Implement clip-aware hit-testing in reverse paint order (`UIEventRouter::HitTest`)
- [x] 5.3 Implement `UIEventRouter` dispatch plus focus tracking and pointer capture for drags
- [x] 5.4 Expose the context `WantsInput` suppression flag (framework wiring lands with integration)
- [x] 5.5 Add `UIEventTest` cases for topmost hit, clipped-element exclusion, focused keyboard delivery, and captured drag

## 6. UI core: widget primitives

- [ ] 6.1 Implement `Panel` container widget with clipping and theme background
- [ ] 6.2 Implement `Image` widget using a texture handle
- [ ] 6.3 Implement `Button` with press/release/click states and theme-driven visuals
- [ ] 6.4 Implement `Text` consuming the text layout output
- [ ] 6.5 Add `UICoreTest` cases for button click and image texture-handle draw command

## 7. UI data: documents, loader, binding, C++ builder

- [ ] 7.1 Define the UI document model (node `type` / `name` / `props` / `bindings` / `children`) with schema version and parse diagnostics
- [ ] 7.2 Implement the `UIElementRegistry` (type-name to factory) and register the core widget primitives
- [ ] 7.3 Implement `UIDocumentLoader` parsing a JSON document into a `UIElement` tree with name lookup
- [ ] 7.4 Make styles/themes data-loadable and resolve document style-class references through the active theme
- [ ] 7.5 Implement `UIDataContext` (named values + change notification) and `IUIDataProvider` path resolution seam
- [ ] 7.6 Implement `UIBinding` (source path to target property) with re-evaluation on context change/frame and diagnostics for unknown paths
- [ ] 7.7 Resolve document texture/font/sub-document references through `IUIAssetResolver` with missing-asset diagnostics
- [ ] 7.8 Implement the C++ `UIBuilder` declarative API sharing the widget/factory layer for loader parity
- [ ] 7.9 Add `UIDataTest` target covering document parse, unknown-type and missing-asset diagnostics, theme swap, binding update, and builder/loader parity

## 8. UI text

- [ ] 8.1 Implement `IUIFontProvider` registration plus a built-in fallback provider; add an optional FreeType-backed provider gated by `SKY_BUILD_FREETYPE` (no hard core dependency)
- [ ] 8.2 Implement glyph rasterization, cache lookup, and atlas page packing with a registered texture handle per page
- [ ] 8.3 Implement atlas growth creating new pages while keeping existing handles valid
- [ ] 8.4 Implement text measurement (single-line width/height, multi-line height)
- [ ] 8.5 Implement text layout producing `UIDrawData` with atlas handle and clip rect
- [ ] 8.6 Add `UITextTest` target covering registration, atlas reuse, atlas growth, and measurement

## 9. Aurora UI render pass

- [ ] 9.1 Author `engine/ui/render/assets/shaders/ui/aui_gui.slang` and `aui_text.slang` (Aurora per-tier slang layout) with `RgBlockDesc` blocks; leave legacy `assets/shaders/ui/*.hlsl` untouched
- [ ] 9.2 Define the pass block (set 1: screen size/projection, time, UI constants) via `RgBlockDesc`; the UI system declares no Global (set 0) block (global binding, if any, is handled by the separate UI pass design)
- [ ] 9.3 Define the batch block (set 2: transform, color tint, uv rect, clip params) and write it with `BatchPackWriter`
- [ ] 9.4 Implement `UIRenderPass : sky::aurora::PipelinePass` with `OnSetup`/`BuildRDG`/`OnSceneChanged`
- [ ] 9.5 Upload `UIDrawData` vertices/indices into per-frame buffers via the Aurora upload path
- [ ] 9.6 Push a `DrawItem` per batch into a UI queue, binding PSO and splitting draws on texture change
- [ ] 9.7 Bind UI atlas images and samplers through Aurora resource groups and descriptor writes
- [ ] 9.8 Apply each draw command's clip rect as a scissor rectangle
- [ ] 9.9 Implement double/triple-buffered per-frame UI GPU data aligned with `DeviceFrameContext` in-flight semantics
- [ ] 9.10 Create the UI `GraphicsPipeline` (`Device::CreatePipelineState` with pipeline state + shader + attachment format); this is the first real PSO creation path in the pipeline layer

## 10. Integration and validation

- [ ] 10.1 Implement `IUITextureRegistry`/`IUIFontProvider`/`IUIAssetResolver` bindings in `UIRender` and register the pass with the renderer
- [ ] 10.2 Host `UIRenderPass` with a test-local graph driver for the smoke test (Aurora has no top-level renderer yet); keep the pass free of driver logic so `aurora-renderer` can absorb it later
- [ ] 10.3 Add `UIRenderTest` target verifying draw-data to `DrawItem` translation and texture-switch splitting (headless/validation path)
- [ ] 10.4 Run a Vulkan end-to-end smoke scene rendering a document-loaded screen (panel, text, image, button) with a data binding
- [ ] 10.5 Verify graceful behavior when a backend lacks required Aurora capabilities (no crash, reported limitation)
- [ ] 10.6 Update `engine/aurora/AGENTS.md` follow-up table and README with the UI system module map
