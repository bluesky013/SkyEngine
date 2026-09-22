## Context

`engine/sandbox/render` (`EditorRender`) exists as a scaffold that only logs. Aurora already implements the layers
needed to drive it without the unfinished engine top-level renderer (`aurora-renderer` is still a TODO): `Aurora.RHI`
(device, command buffers, encoders, swapchain, semaphores), `Aurora.Pipeline` (passes), and `ClientViewport`
(`aurora/rdg/ClientViewport.h`) which owns the `SwapChain` and per-frame acquire/render-done semaphores.
`AuroraModule` (`engine/aurora/adaptor/src/AuroraModule.cpp`) already demonstrates the full init/tick/shutdown
path: `Instance::Get()->Init` → `GetDevice` → `CreateFrameContext` → `CreateCommandPool` → `ClientViewport::Init`
with `SwapChain::Descriptor.window`, then per frame `BeginFrame → Acquire → barrier → clear → barrier → Submit →
Release → EndFrame`. The engine already hosts modules this way: `engine/launcher` uses `GameApplication` +
`ModuleManager` and loads `configs/modules_game.json` (registering `AuroraRender`); `configs/modules_editor.json`
exists (currently pointing at the legacy `SkyRender.Editor` set) and the legacy `EditorApplication` loaded it.
`engine/ui/core` (`sky::ui`) produces `UIDrawData`; `engine/ui/render` (`UIRender`) is a placeholder.

## Goals / Non-Goals

**Goals**
- `EditorRender` initializes the Aurora device and a `ClientViewport` for the editor main window.
- A per-frame clear + present loop, verified visually against the editor window.
- A GUI pipeline that consumes `sky::ui` `UIDrawData` and draws it (quad first; glyphs next).
- Prove the pipeline with a visible quad/rect drawn from a minimal `sky::ui` document.
- Stay independent of the engine scene pipeline; keep one device.

**Non-Goals**
- The engine scene pipeline / 3D viewport rendering (`aurora-renderer`).
- Docking/panels, ImGui overlays, PIE.
- A second device or shared textures.

## Decisions

1. **Reuse the single Aurora device.** `EditorRender` initializes Aurora exactly as `AuroraModule` does
   (`Instance::Get()->Init` → `GetDevice` → `CreateFrameContext` → `CreateCommandPool`). If a device already
   exists (for example the module was initialized by a host), it is reused rather than duplicated; no second
   device is created.
2. **`ClientViewport` from the native handle.** `SwapChain::Descriptor.window = EditorRenderer::Init(windowHandle)`
   with the editor main window handle, width/height from the window; resize goes through `SwapChain::Resize` and
   the viewport is rebuilt (old `GetImage` pointers are invalidated).
3. **Frame loop modeled on `AuroraModule::Tick`.** `BeginFrame` → `Acquire` → image barrier (UNDEFINED →
   COLOR_ATTACHMENT) → render → image barrier (COLOR_ATTACHMENT → PRESENT) → `Submit` (wait acquire, signal
   render-done, frame fence) → `Release` → `EndFrame`.
4. **The GUI pipeline lives in `UIRender` (`engine/ui/render`), not in `EditorRender`.** `UIRender` owns the UI
   shaders, PSOs, batching, and the UI render-graph pass, consuming `sky::ui` `UIDrawData` (textures through
   `IUITextureRegistry`, glyphs through `UIFontAtlas`). `EditorRender` (sandbox) **consumes** `UIRender` and owns
   only the editor frame/host and previews, so the runtime UI and the editor share one UI renderer instead of two.
5. **Pipeline shape.** One textured-quad PSO covers solid rects, images, and glyphs (solid color uses a 1x1 white
   texture); a single vertex format (position, uv, color, plus element params as needed), an index buffer, and
   batching that flushes on texture/clip change. Clip/scissor maps to the encoder scissor; alpha blending on,
   depth off.
6. **Shaders (slang).** UI quad (and later glyph) shaders authored in `engine/shader` style; binding via Aurora
   shader reflection + `ResourceGroup`, matching `aurora-resource-group` conventions (no separate layout object).
7. **Milestones first.** M1 clear/present (visual), M2 one quad from a `sky::ui` document's draw data, M3 glyphs.
8. **Render host target.** A single non-Qt host (`SandboxEditor`) drives `EditorRender`; the earlier RHI-free
   `Sandbox` shell was removed.
   Module deploy dependencies (`sky_add_dependency(... DEPENDENCIES ...)`) are registered for the RHI modules used.
9. **Host through the engine application/module system (launcher pattern).** The editor is a normal engine
   application: `Application`/`GameApplication` + `ModuleManager` loads `configs/modules_editor.json`.
   `SandboxModule` (`IModule`, SHARED, in `engine/sandbox`) is a **thin adapter** over the module lifecycle: it
   owns an `EditorRenderer` and delegates `Init`/`Start`/`Tick`/`Shutdown` to it. No render details live in the
   module. `GameApplication` gains a config override so the editor selects `modules_editor.json` (today it
   hardcodes `modules_game.json`; the legacy `EditorApplication` already loaded `modules_editor.json`). The
   existing sandbox shell was removed; `SandboxEditor` is the single host.
10. **`EditorRender` owns the frame; there is one frame context.** `EditorRenderer` (`engine/sandbox/render`)
    owns the Aurora device + frame context + command pool + main-window `ClientViewport` and runs the frame
    (scene pass + UI pass + present); it calls `UIRenderer` (`UIRender`) for the GUI pipeline. The editor config
    loads `SandboxModule` (→ `EditorRenderer`) and does **not** also load the game's `AuroraRender` clear/present
    module, so there is a single `DeviceFrameContext`. When `aurora-renderer` lands it becomes the single
    top-level renderer and `EditorRenderer` contributes passes to it instead of owning the frame.
11. **A viewport owns its presentation.** A viewport (the render view) is described by
    `{ contentSource, interaction, presentation, overlays }`, and it **maintains its own `presentation`**:
    - `TEXTURE`: render to an offscreen content target sampled by a `sky::ui` image element (composited in the
      editor frame; no swapchain).
    - `WINDOW`: render to the viewport's own `NativeWindow` + `ClientViewport` swapchain.
    - `SHARED_TEXTURE`: consume a GPU-shared texture produced by another process (PIE).
    The **content target is independent of presentation** and is preserved when presentation changes: switching
    (for example TEXTURE <-> WINDOW) creates/destroys the window + swapchain but keeps the target. The main editor
    viewport defaults to `TEXTURE`; previews and detached views may use `WINDOW`. The viewport decides its
    presentation; the system only supplies the device and native windows.
12. **Multiple viewports, one device.** The editor supports N viewports (the main viewport plus previews), each
    with its own content target and its own presentation, all on the single Aurora device (one device, many
    targets and, for `WINDOW`, many swapchains). Thumbnails/bakes are a related but distinct work class: one-shot
    offscreen `RenderJob`s (no live presentation), so `aurora-renderer` must support both live multi-scene/multi-
    view frames and one-shot offscreen renders.
13. **Design the renderer for multiple scenes/views from the start.** World/scene/entity are already
    instance-scoped (`aurora::RenderScene` owns its `EntityRegistry` and views; `aurora::SceneView` is a per-view
    camera; `framework::World` is created via `World::CreateWorld()`; the legacy `framework::EntityManager`
    singleton is unused). So the renderer and the future `aurora-renderer` SHALL handle N `RenderScene`/`SceneView`
    per frame, which keeps an in-process PIE (multiple worlds, one device) possible later. This change does not
    require the engine to become re-entrant, but it must not make it impossible.

## Re-entrancy and PIE process model (deferred)

Re-entrancy is not blocked by the data model. It is blocked only by:

- the top-level renderer not existing yet (`aurora-renderer`), which must support multiple `RenderScene`/
  `SceneView` (Decision 13);
- **non-idempotent registries** (`SerializationContext::Register`, `ComponentFactory`) that assert on duplicate
  registration, so two engine initializations in one process would collide;
- **shared-service ownership** if the editor and a runtime shared a process: `Logger` output callback,
  `CommandRegistry`, and the `Event<T>` buses are process-wide.

Legacy global state (`render::Renderer`, `RHI`, `TextureManager`, `*Feature` singletons) is being replaced by
Aurora and is the only genuinely per-instance-state-wrongly-global part of the render layer.

Therefore the PIE **process model is not decided here**: with the data model already multi-world and
`aurora-renderer` still unwritten, both options stay open —
1. **in-process multi-world** (one device, multiple `RenderScene`/`World`, render to offscreen targets, composite;
   no IPC, no shared texture; a crash takes the editor with it), and
2. **separate process** (isolation; needs IPC + a shared texture to composite the game frame).
The previously archived `editor-shell-redesign` tasks assumed a separate process; that assumption is not binding
and should be revisited in a dedicated PIE change once `aurora-renderer` exists.

## Shader pipeline (dependency)

The engine currently has **no wired shader pipeline**: the RHI pieces exist
(`Device::CreateShaderFunction`/`CreateShader`/`CreatePipelineState`/`CreateResourceGroup`, encoder
`BindPipeline`/`BindVertexBuffers`/`BindIndexBuffer`/`SetScissor`/`Draw`/`DrawIndexed`, `BlitImage`), but nothing
builds a drawable shader from source, and the engine's own passes say so
(`PipelinePass::GetPassShader()` returns `nullptr` — "pass shader not wired yet"; `TextureToScreenPass::OnSetup`
is a TODO; only `GlobalBlock.slang` is code-generated, with no VS/PS bytecode). The full chain has 8 steps:

1. **Authoring** — slang sources per tier: `global` (set 0), `pipeline` (set 1), `batch` (set 2).
2. **Offline codegen** — slang -> SPIR-V / DXIL / MSL bytecode + reflection, emitted as C++ headers
   (`ShaderHeaderTool` today only reflects the global block).
3. **Runtime loading** — build `ShaderFunction`(vs/ps) + `Shader`(+reflection) from the generated data.
4. **Reflection -> descriptor layout** — `ShaderReflection` drives the `ResourceGroup` sets/bindings.
5. **PSO creation** — `GraphicsPipeline::Descriptor` (shader + `PipelineState` + attachment formats).
6. **Per-frame binding** — bind pipeline, resource groups (set 0/1/2), vertex/index buffers, scissor, draw.
7. **Backends** — Vulkan=SPIR-V, DX12=DXIL, Metal=MSL; cross-compiled per target.
8. **Variants / cache / hot reload** — `ShaderVariant` permutations + `ShaderCacheManager`.

For the editor UI we only need a minimal slice:

- **M1 (done, hardcoded in `SandboxModule`)** — runtime `ShaderCompilerSlang::Compile` -> SPIR-V -> Shader ->
  `GraphicsPipeline` -> `Draw(3)`. This proves the shader/PSO/draw path on Vulkan and supplies the plumbing the
  engine passes were missing.
- **M2 (needed to draw real UI)** — add vertex input + texture to the UI shader and **consume `sky::ui`
  `UIDrawData`**: upload `vertices`/`indices` to dynamic buffers, `DrawIndexed` per `UIDrawCmd` with
  scissor = `clip`, bind textures via `ResourceGroup` (`IUITextureRegistry`, white texture + font atlas). Then
  move this from `SandboxModule` into `UIRender`.
- **M3 (production pipeline, belongs to `aurora-renderer`)** — offline codegen (VS/PS bytecode + reflection
  headers), runtime loading without a runtime slang dependency, reflection-driven `ResourceGroup`, multi-backend
  (DXIL/MSL), variants/cache/hot-reload. Suggested as a separate `aurora-shader-pipeline` change.

## UI conventions (projection, clip space, vertex color)

- **The UI renderer uses `Matrix4` with the row-vector convention.** Storage is row-major (`m[0..3]` are the
  four rows), but the *transform convention is a renderer/shader usage decision, not a property of `Matrix4`*:
  this renderer treats a vector as transforming `v' = v * M` (`operator*(Vector4)` = `v.x*row0 + v.y*row1 +
  v.z*row2 + v.w*row3`), so translation lives in the **4th row** (`m[3].xyz`); `A * B` applies A first, then B.
  Putting the translation in the 4th column silently drops it. (Documented here and in `UIRenderer.h`, not in
  core math.)
- **UI projection (pixel -> clip).** `UIDrawData` vertices are pixel-space with a top-left origin (y grows down).
  `UIRenderer::UpdateDrawData` applies an orthographic projection: `x_clip = 2x/w - 1`, `y_clip = 2y/h - 1`. The
  GPU viewport transform then maps clip `(-1,-1)` to the framebuffer **top-left** (Vulkan/DX12 framebuffer origin
  is top-left), so **no extra Y flip is needed**. The projection is baked on the CPU today (the shader passes the
  vertex through); if it moves to a uniform later, the same row-vector matrix applies.
- **Vertex color is ABGR.** The `F_RGBA8` vertex attribute is `VK_FORMAT_R8G8B8A8_UNORM`, so the GPU reads the
  bytes as R,G,B,A; `UIVertex::color` must therefore be packed `0xAABBGGRR` (R in the low byte). Packing RGBA
  swaps R/B and renders wrong colors.
- **Text uses the UI text subsystem.** `UITextSystem` (`engine/ui/core`) bundles an `IUIFontProvider`, an
  `IUITextureRegistry` (the `UIRenderer`), and a `UIFontAtlas`; `UITextLayout::Emit` emits glyph quads that
  reference the atlas page texture, so glyph pages register/upload through the same `IUITextureRegistry` seam and
  draw with the normal UI pipeline. `FreeTypeUIFontProvider` is used when `SKY_BUILD_FREETYPE` is on (the built-in
  provider is the fallback).

## Risks / Trade-offs

- **Shader + reflection binding**: UI shaders must match Aurora's reflection-derived `ResourceGroup`; a mismatch
  fails pipeline creation. Mitigation: start from the simplest quad shader and validate against an existing pass.
- **Vertex format / batching bugs**: wrong stride/offset or premature flush. Mitigation: unit-test the batching
  logic headlessly (CPU-side vertex/index assembly) before wiring the GPU draw.
- **Swapchain resize / OUT_OF_DATE**: `SUBOPTIMAL` is treated as OK and `OUT_OF_DATE` triggers `Resize`; the loop
  must handle a null backbuffer / zero extent (skip the frame).
- **Text/glyph scope**: glyphs need a font atlas; keep quads as the milestone gate and land text next.
- **Device ownership**: if a host already initialized Aurora, double-initialization must be avoided (reuse the
  existing device).
- **Embedded previews cost an extra render target + a UI sampling pass**: each embedded preview adds an offscreen
  target. Mitigation: size targets to the panel, reuse transient targets where possible, and cap the default
  number of previews.
- **Mode switching hazards**: detach/attach creates or destroys a swapchain mid-frame. Mitigation: perform the
  transition at a frame boundary and keep the content target alive across the switch.

## Migration Plan

```
M1  Clear/present: EditorRender inits device + ClientViewport, clears and presents
    the editor window (visual check)
M2  GUI pipeline: quad shader + PSO + batching; draw a visible rect from a minimal
    sky::ui document's UIDrawData
M3  Glyph pipeline + font atlas; draw text
```
Rollback: `EditorRender` is opt-in (built under `SKY_BUILD_SANDBOX`); the shell and `EditorCore` do not depend on
it, so a failed renderer does not block the non-render work.

## Open Questions

1. Text/glyphs in this change or the next?
2. Which RHI backends must the milestone cover (DX12 only on Windows first, or Vulkan too)?
