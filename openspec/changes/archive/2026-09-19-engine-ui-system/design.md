## Context

SkyEngine is on branch `dev_refactor_rhi`: Aurora (`engine/aurora/`, namespace `sky::aurora`) replaces the legacy
`engine/render` path. Aurora provides `Aurora.RHI`, `Aurora.Shader` (slang), `Aurora` (scene ECS) and
`Aurora.Pipeline` (pass templates). Rendering is expressed as RDG passes built from `PipelinePass`
(`OnSetup` / `BuildRDG` / `OnSceneChanged`), with the three resource tiers: Global (set 0,
`GlobalRenderResources`), Pass (set 1, `PipelinePass::GetPassBlocks`), Batch (set 2, `BatchAllocator` +
`BatchPackWriter`, consumed via `DrawItem::batchDynamicOffset`). Shader uniform blocks have a single source
of truth, `RgBlockDesc`, which generates both the RHI resource-group layout and HLSL headers.

All current UI is ImGui-based (`engine/render/imgui`) and depends on the legacy render path; there is no
retained-mode, render-agnostic runtime UI. Font rasterization is available through the optional
`FreeTypeModule` plugin (gated by `SKY_BUILD_FREETYPE`; it currently links the legacy `RenderCore`), and legacy
UI shaders exist at `assets/shaders/ui/` (`gui.hlsl`, `text.hlsl`) but target the old pipeline.

Constraints:
- Aurora backends have gaps today (DX12 PSO stub, Metal `ResourceGroup` stub), so the UI renderer must land on
  Vulkan first and stay backend-agnostic at the interface level.
- `sky::aurora` is the only allowed Aurora namespace; no aliases.
- UI core must compile with no render or Aurora includes.
- ASCII-only source content per repository rules; `sky::ui` is the chosen UI namespace.

## Goals / Non-Goals

**Goals:**
- A retained-mode runtime UI system under `engine/ui/`, split into `core` (render-agnostic) and `render`
  (Aurora-backed) submodules.
- A clean seam between the UI data model and rendering, so the core can be unit-tested without a GPU.
- A UI raster pass that records through the Aurora RDG/pipeline model and runs on the Vulkan backend first.
- Text: glyph atlas and text layout behind an `IUIFontProvider` seam, with an optional FreeType-backed provider.
- First widget primitives: container/panel, text, image, and button.
- Data-driven UI: UI documents loaded at runtime into the element tree, with style/theme resources and
  data-context property binding; a C++ declarative builder is available for tests and code-first UI.

**Non-Goals:**
- Editor / Qt integration and any editor UI shell.
- Replacing ImGui for dev/debug tools.
- A full widget library (scroll views, text input, lists, docking, etc.).
- Animation / transitions, accessibility, and non-Latin shaping (HarfBuzz-class) in this milestone.
- A visual UI editor/authoring tool, document-to-code generation, and hot reload.
- Function-backed / attribute bindings (UMG `TAttribute`-class) beyond path-based bindings.
- A full routed event model (capture/bubble phases with event-consumption replies); this milestone provides
  hit-test dispatch, focus/capture, and a simple handled flag.
- Per-widget style structs (UE `FButtonStyle`-class) and a style authoring pipeline; this milestone uses one
  generic `UIStyle`/theme resolved through style classes.
- Incremental (partial) draw-data rebuild; this milestone repaints the full frame from paint dirty flags.
- DPI scaling and localization (`FText`-class text).
- New Aurora RHI features beyond what the UI pass needs.

## Decisions

### 1. Module split and dependency direction
```
engine/ui/
  core/    target UI          -> Core, Framework (no render/aurora includes)
  render/  target UIRender    -> UI, Aurora.Pipeline, Aurora.RHI, Aurora.Shader
engine/test/ui/  targets UICoreTest, UIDataTest, UITextTest, UIRenderTest
                 (repo convention keeps test targets under engine/test/<module>)
```
`UIRender` depends on `UI`; `UI` never depends on `UIRender`. The `engine/ui/core` target holds the element
tree, layout/style, input, widgets, text, and the `ui-data` document layer; `engine/ui/render` holds the Aurora
pass. Core exposes four seams, each implemented outside core (or by fakes in tests): `IUITextureRegistry`
(registers atlas/page textures as opaque handles; implemented by `UIRender`), `IUIFontProvider` (glyph
rasterization; core fallback plus an optional FreeType-backed provider), `IUIAssetResolver` (document
texture/font/sub-document refs; implemented by `render`/`framework`), and `IUIDataProvider` (binding path
resolution; implemented by `framework`/game code).
Alternative considered: single `engine/ui` target with optional render sources — rejected because it lets
render headers leak into core and complicates headless tests.

### 2. Retained-mode element tree
`UIElement` is the base node: parent/child links, a stable id, visibility, enabled flag, bounds, transform,
and layout/paint dirty flags. A single `UIContext` owns the root and drives layout then paint each frame.
Widgets (`Panel`, `Text`, `Image`, `Button`) derive from `UIElement`; these class names double as the document
`type` keys. Alternative considered: immediate-mode — rejected by the user decision to keep retained semantics
for data binding, hierarchy, and incremental invalidation.

### 3. Layout and style separation
Layout is a two-pass measure/arrange box model with anchor/size rules; style/theme is a separate
`UIStyle` value struct resolved from a theme rather than stored on widgets. This keeps widget logic small and
makes theming data-driven. Alternative considered: hard-coded widget styles — rejected for maintainability.

### 4. Render-agnostic draw data (the core/render contract)
Core paint produces an `UIDrawData` describing a frame in UI coordinates:
```
UIDrawData {
  UIVertex   { pos, uv, color }          // batched triangle list
  uint32     indices[]
  UIDrawCmd  { indexOffset, indexCount, clipRect, textureId }
}
```
`textureId` is an opaque core-side handle resolved by the render module. Core never sees Aurora types.
This mirrors the well-understood ImGui draw-data shape and keeps the renderer a pure translator.
Alternative considered: have core emit RDG passes directly — rejected (couples core to Aurora).

### 5. Aurora render pass
`UIRenderPass` derives from `sky::aurora::PipelinePass` and records a raster pass via
`RenderGraph::AddSceneRasterPass` (UI has no scene geometry, so it does not use
`SceneRasterPassTemplate`'s scene queues). Each frame:
- Upload `UIDrawData` vertices/indices into a transient/per-frame buffer (Aurora upload path).
- For each `UIDrawCmd`, push a `DrawItem` (`pso`, `vb`/`ib`/offsets, `batchDynamicOffset`,
  `CmdDrawIndexed`) into a single UI queue, switching PSO/descriptor state on texture change.
- Clipping uses `clipRect` -> scissor for axis-aligned clips; shader-side clip parameters cover nested clips
  if needed later.

This UI system prescribes uniform data only for the pass and batch tiers (`RgBlockDesc` single source of
truth):
- Pass (set 1): screen size / projection, `time`, and UI constants; sampler + atlas texture bound via
  resource-group descriptor writes (textures/samplers are not UBO blocks, so they are not `RgBlockDesc` fields).
- Batch (set 2): per-draw transform, color tint, uv rect, clip params, written through `BatchPackWriter`
  and consumed as `DrawItem::batchDynamicOffset`.

The Global tier (set 0) is deliberately **not** governed by this UI system design. UI works in screen space, so
it does not require scene view/projection, but whether a UI pass binds an optional global resource group (e.g.
for a separate overlay/present composition) is a decision for the separate UI pass integration design. The
Aurora executor binds set 0 only when a global resource group is present (`Execute.cpp`), so a UI graph without
one simply leaves set 0 unbound; this system does not impose either choice.

Shaders are authored for the Aurora slang path and owned by the UI render module under
`engine/ui/render/assets/shaders/ui/` (`aui_gui.slang`, `aui_text.slang`), following Aurora's per-tier slang
layout, with HLSL headers generated from `RgBlockDesc`. The legacy root `assets/shaders/ui/*.hlsl` are left
untouched. Alternative considered: reuse the legacy HLSL — rejected (old shader backend and manual layout).

### 6. Text capability
`ui-text` (inside `engine/ui/core`) owns font registration, glyph atlas pages, metrics, and text layout to
`UIDrawData`. Rasterization is delegated to an `IUIFontProvider`, so core has no hard font dependency. Core
ships a built-in bitmap fallback provider (text works headless and without plugins). A FreeType-backed provider
is supplied by an optional add-on/plugin when `SKY_BUILD_FREETYPE` is enabled, adapted to `IUIFontProvider`
without pulling the legacy `RenderCore` dependency into core. Atlas pages are registered with
`IUITextureRegistry` and uploaded by `UIRender` through the Aurora upload path. Alternative considered: SDF
text from day one — deferred; bitmap atlas first.

### 7. Input and events
`UIInputState` (pointer positions, buttons, wheel, key events, IME text) is fed by the framework input layer.
`UIEventRouter` performs hit-testing against the element tree (reverse paint order, clip-aware), dispatches
`PointerEvent` / `KeyEvent`, and maintains focus + capture. The console/`IConsoleUI`-style global input
suppression (`WantsInput`) is honored so game input can be gated. Only the default single-pointer path is in
scope; multi-touch is a follow-up.

### 8. Frame data lifetime
Layout/paint run on the main thread; the recorded `UIDrawData` is double/triple-buffered so the render thread
can consume the previous frame while the main thread builds the next. Buffers are per-inflight-frame, matching
`DeviceFrameContext` semantics.

### 9. Data-driven UI documents, binding, and C++ declarative parity
UI is authored as data first, with C++ as an equal-producing alternative for tests and code-first screens.
This mirrors how UMG (serialized `WidgetTree` + property/event bindings over Slate attributes) and Cocos2d-x
(Cocos Studio `.csb`/`.json` deserialized by `CSLoader::createNode`) separate the UI definition from runtime
widget code.

- **Placement**: the document model, loader, binding, and C++ builder live in the `engine/ui/core` target as the
  `ui-data` capability (render-agnostic); only rendering the resulting tree lives in `engine/ui/render`.
- **Document format**: a UI document is a declarative tree of elements. Each node carries `type`, `name`,
  `props` (grouped layout/visual/content values), optional `bindings`, and `children`. Documents reference
  style/theme classes rather than raw colors so theming stays swappable. Format is JSON (consistent with
  `rapidjson` in the repo); a compiled/binary cache is a later optimization, not this milestone.
- **Loader and element-type registry**: `UIDocumentLoader` parses a document and builds a `UIElement` tree
  through an extensible `UIElementRegistry` (type name -> factory). Unknown types and missing assets are
  reported as load diagnostics rather than silent failures. This is the `CSLoader` role.
- **Style/theme resources**: `UIStyle`/`UITheme` are loaded as data (Slate `StyleSet` role). Documents resolve
  appearance through style classes; replacing the theme re-skins all documents without editing them.
- **Data binding**: a `UIDataContext` exposes named values and change notifications. A `UIBinding` maps a
  source path on the data context to a target element property; bindings re-evaluate on context change or per
  frame. This is the UMG/Slate property-binding role. Event bindings and an event graph are explicitly out of
  scope. Path resolution goes through a small `IUIDataProvider` seam so the milestone does not hard-depend on
  the reflection system (direct reflection integration is an open question).
- **Asset resolution seam**: documents reference textures/fonts/sub-documents by id or path. The loader
  resolves refs through `IUIAssetResolver`; resolved textures become handles registered with
  `IUITextureRegistry`, and fonts become `IUIFontProvider` registrations. Both seams keep `ui-core`/`ui-data`
  render-agnostic; `render`/`framework` implements them.
- **C++ declarative builder**: a `UIBuilder` fluent API constructs the same element tree and attaches the same
  bindings. Loader and builder share the widget/factory layer, so a screen built in C++ behaves identically to
  the same screen loaded from a document; tests use the builder to avoid asset I/O.

Alternative considered: codegen from documents into typed C++ classes (UMG widget-blueprint compile, Cocos
Creator script binding) — rejected for this milestone to avoid a build-time toolchain; the document model is
designed so codegen can be added later without changing runtime semantics.

### 10. UI document schema (field-level)

Documents are JSON. Top level:

```json
{
  "schema": 1,
  "name": "MainMenu",
  "resources": { "avatar": "textures/avatar.png", "font_ui": "fonts/ui.ttf" },
  "root": { "type": "Panel", "name": "Root", "children": [] }
}
```

A node:

```json
{
  "type": "Panel",
  "name": "Root",
  "enabled": true,
  "visible": true,
  "style": ["screen.root"],
  "props": {
    "layout": { "anchor": [0, 0, 1, 1], "offset": [0, 0, 0, 0], "size": ["fill", "fill"], "pivot": [0, 0], "padding": [8, 8, 8, 8], "z": 0 },
    "visual": { "color": "#FFFFFF", "opacity": 1.0, "texture": "avatar", "tint": "#FFFFFF" },
    "text":   { "content": "Hello", "font": "font_ui", "size": 16, "align": "left", "valign": "top", "wrap": true },
    "button": { "interactable": true }
  },
  "bindings": [
    { "target": "text.content", "source": "player.name" },
    { "target": "visual.opacity", "source": "player.health", "converter": "remap", "args": { "in": [0, 100], "out": [0.3, 1.0] } }
  ],
  "children": []
}
```

Field tables:

| Document field | Type | Required | Notes |
|---|---|---|---|
| `schema` | int | yes | schema version; loader rejects versions it does not know |
| `name` | string | no | document / debug name |
| `resources` | object | no | id -> path alias map, resolved through `IUIAssetResolver` |
| `root` | node | yes | root element of the tree |

| Node field | Type | Required | Notes |
|---|---|---|---|
| `type` | string | yes | registry key: `Panel` / `Text` / `Image` / `Button` in this milestone |
| `name` | string | no | unique within the document; required if looked up or used as a binding anchor |
| `enabled` / `visible` | bool | no | default `true` |
| `style` | string or string[] | no | style classes, resolved in order (later overrides earlier) |
| `props` | object | no | grouped property bag (below) |
| `bindings` | array | no | binding entries (below) |
| `children` | array | no | child nodes in declaration order = paint order |

Property groups (`props`):
- `layout`: `anchor` (normalized vec4 minX/minY/maxX/maxY), `offset` (vec4 left/top/right/bottom), `size`
  (`"fill"` / `"auto"` / `[w, h]`), `pivot` (vec2), `padding` (vec4), `z` (int).
- `visual`: `color` (`#RRGGBB`/`#RRGGBBAA`), `opacity`, `texture` (resource ref), `tint`, `nineSlice`.
- `text`: `content`, `font` (resource ref), `size`, `align`, `valign`, `wrap`, `color`.
- `button`: `interactable`.
- Unknown groups or keys produce a warning diagnostic and are ignored.

Value precedence (lowest to highest): theme defaults < `style` classes (in order) < explicit `props` <
binding-evaluated values.

Binding entry:
- `target`: dotted property path on the node (`<group>.<key>`, e.g. `text.content`); MUST resolve to a
  bindable property of that element type, otherwise an error diagnostic.
- `source`: dotted path resolved by `IUIDataProvider` against the active data context.
- `converter`: optional registered converter name; `args` is an object. Built-ins: `remap`, `format`,
  `boolToVisible`.
- `mode`: `OneWay` (default) or `OneTime`; `TwoWay` is reserved and out of scope.
- Unknown `source` path -> warning diagnostic; the target keeps its last value.

Loading result (loader never throws on content errors; it returns diagnostics):

```cpp
struct UIDiagnostic { UIDiagnosticSeverity severity; std::string nodePath; std::string code; std::string message; };
struct UIDocumentLoadResult { UIElementPtr root; std::vector<UIDiagnostic> diagnostics; };
```

Element lookup by `name` is available on the loaded root (e.g. `FindByName`). Duplicate names within a
document produce an error diagnostic.

C++ builder parity (same tree and bindings as the document loader):

```cpp
UIBuilder b;
auto &root = b.Root<Panel>("Root").Style("screen.root").Anchor(0, 0, 1, 1).Size("fill", "fill");
b.Child<Text>("Title").Content("Hello").Bind("text.content", "player.name");
b.Child<Image>("Avatar").Texture("avatar");
UIElementPtr tree = b.Build();
```

## Risks / Trade-offs

- **Aurora top-level renderer is under parallel development** -> `engine/aurora/core/include/aurora/Renderer.h` is
  still a TODO stub (`aurora-renderer` change not landed) and Aurora is not yet wired into the launcher/editor
  (still legacy `render/Renderer.h`), so the UI pass cannot run end-to-end until that lands. Mitigation: keep
  `UIRenderPass` a pure `PipelinePass` with no driver logic so it integrates through a stable seam; for the
  smoke test host it with a test-local graph driver (as the pipeline tests do) rather than shipping a competing
  renderer; the pass is handed to `aurora-renderer` when ready.
- **First real PSO creation path** -> `OpaquePass::OnSetup` still has a TODO for the persistent PSO, so no
  pipeline pass creates a `GraphicsPipeline` today. The UI pass will exercise
  `Device::CreatePipelineState(GraphicsPipeline::Descriptor)` (`state` + `shader` + `AttachmentFormat`) plus
  shader/variant resolution for the first time. Mitigation: land the UI pass against a minimal pipeline state
  and treat PSO creation as an explicit, separately testable step.
- **Aurora backend maturity (DX12 PSO stub, Metal `ResourceGroup` stub)** -> scope this milestone to Vulkan;
  keep `UIRenderPass` free of backend-specific branches; add DX12/Metal behind the same pass once the Aurora
  follow-ups (`aurora-resource-group`, DX12 PSO) land.
- **Texture upload path (`aurora-upload`) may be incomplete** -> start with a single atlas texture and the
  simplest upload; fall back to CPU-written transient buffer if bulk upload is unavailable.
- **Batch-tier dynamic UBO alignment / stable binding contract** -> follow `BatchPackWriter` and
  `minUniformBufferOffsetAlignment`; size UI batch data to alignment and avoid `range==0` dynamic descriptors.
- **Nested clipping** -> scissor only supports axis-aligned, non-rotated clips; if rotations or arbitrary
  clips are required, add shader-side clip params (already reserved in the batch block).
- **Font/IME complexity** -> bitmap atlas + Latin first; non-Latin shaping and full IME are explicit follow-ups.
- **Two UIs (ImGui + new UI) coexist** -> keep modules independent; no shared renderer state; document that
  ImGui remains the dev-tools path.
- **Document schema drift / versioning** -> version the document schema and validate on load, so old documents
  fail with a clear diagnostic instead of mis-loading; add a binary cache only after the schema stabilizes.
- **Binding cost and lifetime** -> bindings hold no owning pointer to their data source; invalidate on context
  change rather than polling every property; guard against dangling element targets on tree mutation.
- **Reflection coupling** -> keep binding behind `IUIDataProvider` so the milestone does not require the core
  reflection system; reflection integration stays an open question.

## Migration Plan

Not a migration: the new system is additive and leaves `ImGuiRender` / `engine/render` untouched. Rollout
order inside the change: core tree + layout + events -> widget primitives -> UI document model + loader +
binding + C++ builder -> text -> Aurora render pass (Vulkan) -> tests. The render pass smoke test runs through a
test-local graph driver; full engine end-to-end wiring integrates with `aurora-renderer` when it lands (parallel
development). Rollback is removing the `engine/ui` subtree and its CMake entries; no existing behavior changes.

## Open Questions

- Editor integration seam (Qt viewport embedding) — explicitly deferred to a follow-up change.
- Whether text needs a shaping layer (HarfBuzz) and bi-directional text before non-English shipping.
- Authoring format for themes/styles (C++ defaults vs data asset) — this change makes styles data-loadable but
  does not ship an authoring tool.
- Path binding source: a dedicated `IUIDataProvider` first, or direct integration with the core reflection
  system when available.
- Document binary cache format and hot-reload trigger — deferred past this milestone.
- Whether function-backed bindings (UMG/Slate `TAttribute`-class) are added so the C++ builder and document
  paths stay capability-equivalent rather than path-binding only.
- Event routing model: immediate dispatch vs capture/bubble phases, and where event consumption (handled
  propagation) is decided.
- Whether the generic `UIStyle` splits into per-widget style structs as the widget set grows.
- Incremental paint strategy (per-subtree draw caches / invalidation) and its interaction with the paint dirty
  flags introduced in Decision 2.
- DPI scale source and the localization/text model (`FText`-class) for shipping non-English titles.
- Hosting seam for the UI pass until `aurora-renderer` lands (parallel development): test-local driver only, or
  a thin reusable overlay driver that `aurora-renderer` later absorbs.
- Target registration name (`UI`/`UIRender` vs `UI.Core`/`UI.Render`) to confirm against `sky_add_library` conventions.
