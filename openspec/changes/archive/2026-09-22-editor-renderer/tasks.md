## 1. Editor module host

- [x] 1.1 Add a non-Qt editor host (`EditorApplication` + `SandboxEditor` exe in `engine/sandbox/app`) that loads `SandboxModule` (registration hardcoded for now; a `GameApplication` config override can replace it later, which `GameApplication` needs because it force-loads `AuroraRender`)
- [x] 1.2 Add `SandboxModule` (`IModule`, SHARED, `REGISTER_MODULE`, `engine/sandbox/module`): `Init` (Aurora device + frame context + command pool), `Start` (main-window `ClientViewport`), `Tick` (hardcoded frame), `Shutdown`
- [x] 1.3 Rewrite `engine/configs/modules_editor.json` for the new modules (drop the legacy `SkyRender.Editor` set; list `SandboxModule`); a single frame context
- [x] 1.4 Register module deploy dependencies (`sky_add_dependency(TARGET SandboxModule DEPENDENCIES SandboxEditor)` + `sky_set_dependency`)

## 2. Editor renderer base

- [x] 2.1 Init the Aurora device inside the editor module (no second device) and create the `ClientViewport` from the editor main window handle
- [x] 2.2 Hardcoded frame loop (barrier -> scene pass clear -> UI pass -> barrier -> submit -> present); verified running on the editor window (Vulkan)
- [x] 2.3 Handle resize / `OUT_OF_DATE` / null-backbuffer frames (handled by `ClientViewport::Begin()` rebuild-on-`OUT_OF_DATE` plus the Tick guards)
- [x] 2.4 Hardcoded UI pass: `BlitImage` a UI layer image into the backbuffer (first shader-free placeholder)
- [x] 2.5 Runtime-compiled UI shader: `ShaderCompilerSlang` compiles a slang UI shader to SPIR-V at runtime, a `GraphicsPipeline` is created, and the UI pass records `Draw(3)` with a scissor (shader/PSO/draw path proven on Vulkan; this is the plumbing the engine pipeline passes were missing)
- [x] 2.6 Vertex path: UI shader with vertex input (pos/uv/color) + dynamic vertex/index buffers; UI pass binds them and `DrawIndexed` per `UIDrawCmd` (scissor = clip), consuming a hardcoded `sky::ui::UIDrawData`
- [x] 2.7 Texture path: shader samples a texture; 1x1 white `Image` + `Sampler` bound via a `ResourceGroup` (reflection set 0); alpha blend enabled; first-frame staging upload + barriers (Vulkan)
- [x] 2.8 Move the GUI pipeline into `UIRender`: `sky::ui::UIRenderer` owns the shader/PSO/buffers/texture/resource-group and records draws; `SandboxModule` only hosts the frame and feeds draw data
- [x] 2.9 Layering: move the frame flow (device/frame context/command pool/`ClientViewport`/passes/present) into `EditorRender` (`EditorRenderer`); `SandboxModule` is a thin adapter delegating `Init`/`Start`/`Tick`/`Shutdown`

## 3. GUI pipeline (`UIRender`)

- [x] 3.1 Implement the GUI pipeline in `UIRender` (`engine/ui/render`, `sky::ui::UIRenderer`), owned by the engine so the runtime UI and editor share it
- [x] 3.2 Author the UI shader (slang, runtime-compiled to SPIR-V) and bind its texture through Aurora shader reflection + `ResourceGroup`
- [x] 3.3 Vertex/index assembly: `UIRenderer` uploads `UIDrawData` and converts pixel-space vertices to NDC; batching (flush on texture/clip change) is provided by `UIPaintContext`
- [x] 3.4 Record draws between `BeginRendering`/`EndRendering` (alpha blend, depth off, scissor from clip)
- [x] 3.5 Consume `sky::ui` draw data in `EditorRender`: `UIPaintContext` builds a top bar / left panel / translucent overlay and `UIRenderer` draws it (visible on Vulkan)
- [x] 3.6 Text: `UITextSystem` + `UIFontAtlas` wired to the `UIRenderer` registry; `UITextLayout::Emit` draws glyph quads (atlas pages register through `IUITextureRegistry`). Uses `FreeTypeUIFontProvider` (`assets/fonts/OpenSans-Regular.ttf`) when `SKY_BUILD_FREETYPE` is on (built-in provider otherwise); added a `SKY_BUILD_FREETYPE` compile definition on the `UI` target. Verified real anti-aliased text by screenshot. Also fixed a texture-id collision where `RegisterTexture` reused an id taken by `RegisterImage`.
- [x] 3.7 Bind textures through `IUITextureRegistry` (`UIRenderer` implements it: CPU images -> GPU upload + per-texture `ResourceGroup`); `RegisterImage` also accepts GPU images; draws bind the RG per `UIDrawCmd` (white fallback). Headless test for vertex assembly/NDC still pending.
- [x] 3.8 Headless tests: added `UIPaintContextTest` (5 tests) covering vertex assembly, command index offsets (regression for the index/vertex-base bug), batching, clip changes, and texture ids. The pixel->NDC projection is verified visually; a unit test is deferred until it is extracted from the device-coupled `UIRenderer`.

## 4. Viewports (presentation, multiple)

- [x] 4.1 Define the viewport model (`{ contentSource, interaction, presentation, overlays }`) and a `ViewportManager` (create/destroy/list); 4 tests pass
- [x] 4.2 Content target: offscreen `Image` (RENDER_TARGET | SAMPLED) per viewport, rendered as a placeholder (clear) with layout barriers to SHADER_READ_ONLY
- [x] 4.3 `TEXTURE` presentation: sample the content target as a `sky::ui` image element composited into the editor frame (no swapchain). Fixed `UIPaintContext::AddQuad` which set `cmd.indexOffset` to the VERTEX base instead of the INDEX base (broke every command after the first); verified by screenshot.
- [ ] 4.4 `WINDOW` presentation: **scheme C code implemented** (a second command buffer per WINDOW viewport: render target -> clear -> barrier -> `BlitImage` into the window swapchain -> barrier -> present; one `Submit` with both command buffers, combined wait/signal semaphores, shared frame fence; per-viewport `Release()`). Also fixed UI upload ordering (call `EnsureTextureReady` AFTER `PaintUI`, since glyph pages are created lazily while painting and were sampled before upload the first frame). **BLOCKED**: a second OS window cannot be created here — `SDL_CreateWindow` fails with "invalid parameter" for the 2nd window (the 1st works; unrelated to flags / `SDL_WINDOW_VULKAN`). Needs an SDL/host investigation, or create all windows from the host before modules load.
- [ ] 4.5 Runtime presentation switch (`TEXTURE` <-> `WINDOW`) keeping the same content target; create/destroy window+swapchain
- [ ] 4.6 Multiple viewports (main + previews) and per-viewport resize/close on the single device

## 5. Thumbnails / one-shot render jobs (later)

- [ ] 5.1 `aurora-renderer`: support one-shot offscreen render jobs (thumbnail/bake) in addition to live multi-scene/view frames
- [ ] 5.2 Thumbnail job: fixed studio camera/lighting per asset type; render once to a small target
- [ ] 5.3 Thumbnail cache: in-memory LRU (+ optional disk/cook pregeneration); consume via `IUITextureRegistry`
- [ ] 5.4 Budget/queue: limit per-frame thumbnail jobs; size targets and pool/atlas where possible

## 6. Validation

- [ ] 6.1 Visual check: clear color end to end, then a visible quad from draw data; viewport WINDOW present
- [x] 6.2 Headless test of the batching/vertex assembly logic (no GPU): `UIPaintContextTest` in `UICoreTest`
