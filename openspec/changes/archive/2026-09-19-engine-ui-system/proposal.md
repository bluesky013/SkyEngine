## Why

SkyEngine has no first-party runtime UI layer. Existing UI code is ImGui-centric and bound to the legacy
render path (`engine/render/core`), so it cannot run on Aurora's RDG / `PipelinePass` model. Games need a
retained-mode runtime UI (HUD, menus, in-world panels) that renders through Aurora consistently on
Vulkan / DX12 / Metal, while keeping the UI data model free of any render dependency so tooling and future
editor integration can reuse it.

## What Changes

- Introduce a new `engine/ui/` module tree with two submodules:
  - `engine/ui/core` — render-agnostic runtime UI (element tree, layout, style/theme, input routing, focus/hit-test, widget primitives).
  - `engine/ui/render` — Aurora-backed renderer that turns extracted UI draw data into a raster pass through the Aurora pipeline.
- Add a retained-mode element tree with a single root, parent/child hierarchy, visibility, and dirty propagation for layout and paint.
- Add a layout system (anchors + box layout) and a style/theme system decoupled from widgets.
- Add input/event dispatch (pointer, touch, keyboard, IME), including focus capture and hit-testing with clip regions.
- Add widget primitives needed by the first milestone: container/panel, text, image, and button.
- Add a data-driven UI document layer: UI documents (JSON) describing element trees, style/theme references, and
  property bindings, a runtime loader that builds the element tree through an extensible element-type registry,
  and a data-context binding model. A C++ declarative builder SHALL produce the same tree as the document loader.
- Add a text capability: glyph atlas and text layout behind an `IUIFontProvider` seam, with a built-in fallback provider and an optional `FreeTypeModule`-backed provider (`SKY_BUILD_FREETYPE`).
- Add an Aurora render backend for UI: draw-data extraction, quad batching, texture-atlas binding, scissor/clip, and an RDG raster pass built on `RgBlockDesc`-derived shader blocks and the batch tier dynamic UBO.
- Add build targets under `engine/ui/` and wire them into the engine CMake tree.

**Non-goals**: editor/Qt embedding, replacing ImGui for dev tools, a full widget library, an animation/transition
system, a visual UI editor/authoring tool, document-to-code generation, and hot reload. Function-backed
bindings, a full routed event model (capture/bubble + event consumption), per-widget style structs, incremental
draw-data rebuild, and DPI/localization are also deferred. Editor integration is a follow-up change.

## Capabilities

### New Capabilities
- `ui-core`: retained-mode element tree, layout, style/theme, input/event dispatch, focus and hit-testing, and base widget primitives with zero render dependency.
- `ui-data`: UI document format, runtime loader and element-type registry, style/theme resources, data-context property binding, and a C++ declarative builder with loader parity.
- `ui-text`: font registration behind a provider seam, glyph atlas, text measurement/layout, and text draw-data emission.
- `ui-render`: Aurora render backend that consumes UI draw data and records a UI raster pass through the Aurora RDG/pipeline model.

### Modified Capabilities
<!-- None: no existing spec requirements change. -->

## Impact

- New source tree `engine/ui/core`, `engine/ui/render`; new CMake targets registered in `engine/CMakeLists.txt`.
- Depends on `Framework` + `Core` (ui-core) and `Aurora.Pipeline` / `Aurora.RHI` / `Aurora.Shader` (ui-render); no Aurora scene-module dependency.
- Optional `FreeTypeModule`-backed font provider (`SKY_BUILD_FREETYPE`); UI slang shaders live under `engine/ui/render/assets/shaders/ui/`, leaving the legacy `assets/shaders/ui/*.hlsl` untouched.
- Adds tests: `UICoreTest`, `UIDataTest`, `UITextTest`, `UIRenderTest`.
- No changes to existing legacy render modules (`RenderCore`, `RenderAdaptor`, `ImGuiRender`); they remain untouched.
