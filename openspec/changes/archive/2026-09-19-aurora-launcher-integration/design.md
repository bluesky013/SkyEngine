## Context

Launcher boot chain: `Launcher` → `sky::GameApplication` (framework `Application`) → `ModuleManager::LoadModules` dynamically loads modules declared in `configs/modules_game.json` plus a hard-coded render module in `GameApplication::LoadConfigs` (`SkyRender`). A module is a `SHARED` dll exporting `StartModule`/`StopModule` via `REGISTER_MODULE`, implementing `sky::IModule` (`Init`/`Start`/`Tick`/`Shutdown`). Old render: target `SkyRender` → `REGISTER_MODULE(sky::RenderModule)`.

Aurora provides `aurora::Instance` (creates `Device`, dlopens the backend dll), `ClientViewport` (SwapChain + per-image binary semaphores), `DeviceFrameContext` (frame allocator + fence ring), `CommandPool`/`CommandBuffer` + `GraphicsEncoder::BeginRendering` (clear), and `Queue::Submit`. `SwapChain::Descriptor::window` is the native window handle. `Platform::Get()->GetMainWinHandle()` returns it.

## Goals / Non-Goals

**Goals:**
- An `AuroraRender` module the launcher can load, drive per tick, and shut down.
- Minimal end-to-end: open the platform window, create the swapchain, clear the backbuffer each frame, submit + present, handle resize, no crash when uninitialized.
- Coexist with `SkyRender` (user-selectable).

**Non-Goals:**
- `Renderer` main loop, RDG scene, passes/PSO, materials, lighting, skinning.
- Editor viewport integration.
- Replacing `SkyRender` as the default.

## Decisions

### D1. Module location and target

`engine/aurora/adaptor/` → `sky_add_library(TARGET AuroraRender SHARED ...)`, sources `AuroraModule.cpp` + `AuroraRegistry.cpp`, links `Aurora.RHI` + `Framework`. Backend dlls (`AuroraVulkan`/`AuroraDX12`) are declared as dependencies so they land beside the executable; the device crate is dlopened at runtime by `Instance`.

### D2. `AuroraModule : sky::IModule`

- `Init(args)`: parse `-r rhi` → `aurora::API` (default `DEFAULT`); `Instance::Get()->Init({appName, engineName, debugLayer, api})`; `device = Instance::Get()->GetDevice()`; create `DeviceFrameContext` (`inflightNum = 1`, `parallelNum = 1`); create and `Init()` a GRAPHICS `CommandPool`; allocate one `CommandBuffer` (reused because `inflightNum == 1` serializes frames).
- `Start()`: `window = Platform::Get()->GetMainWinHandle()`; `viewport = ClientViewport("main")`; `viewport->Init(device, {window, width, height, BGRA8_UNORM, IMMEDIATE})`.
- `Tick(delta)`: the loop in D3.
- `Shutdown()`: destroy viewport/command buffer/pool/frame context, `Instance` teardown.

### D3. Minimal present loop

Per `Tick`:

1. `frameContext->BeginFrame()` (waits + resets the slot fence).
2. `if (!viewport->Begin() || !viewport->Acquire()) return;` (resize/minimized/acquire-fail ⇒ cancel the frame; `EndFrame` still runs).
3. `cmdBuf->Begin()`; barrier backbuffer `UNDEFINED → COLOR_ATTACHMENT` (`srcAccess = NONE`, `dstAccess = RTV`, `srcStage = TOP`, `dstStage = COLOR_OUTPUT`).
4. `GraphicsEncoder::BeginRendering({renderArea, 1 color = backbuffer, LoadOp::CLEAR, StoreOp::STORE, clearValue})`; `EndRendering()`.
5. barrier `COLOR_ATTACHMENT → PRESENT` (`srcAccess = RTV`, `dstAccess = PRESENT`, `srcStage = COLOR_OUTPUT`, `dstStage = BOTTOM`).
6. `cmdBuf->End()`.
7. `Queue::Submit`: wait `viewport->GetAcquireSemaphore()` (stage `COLOR_OUTPUT`), signal `viewport->GetRenderDoneSemaphore()` (stage `BOTTOM`), `fence = frameContext->GetFrameFence()`.
8. `viewport->Release()` (present waits the render-done semaphore).
9. `frameContext->EndFrame()`.

`oldLayout = UNDEFINED` every frame is safe because the pass clears; VK discards contents, DX12 ignores layout. D3D12 maps access→state.

### D4. Launcher selection

`GameApplication` gains a renderer selection (`--renderer sky|aurora`, default `sky`) parsed alongside `-p`; `LoadConfigs` registers `ModuleInfo{renderModule, {}}` instead of the hard-coded `SkyRender` (`SkyRender` keeps its `ShaderCompiler` dependency; `AuroraRender` has none).

### D5. Failure safety

`Init` failure leaves `device == nullptr`; `Tick`/`Shutdown` are no-ops in that case. `viewport` null or unavailable cancels the frame without presenting. `Begin`/`Acquire` return false on OUT_OF_DATE/LOST/minimized and the loop skips present.

## Risks / Trade-offs

- **`inflightNum = 1`** serializes CPU/GPU; fine for the first milestone, raise to 2+ with a command-buffer ring later.
- **Barrier correctness** depends on backend swapchain layout handling; VK does not auto-transition, so the explicit barriers in D3 are required. DX12 ignores layout but needs the state transition (PRESENT↔RENDER_TARGET), which the access mapping provides.
- **Module name collision**: `AuroraRender` must not clash with `SkyRender`; both can be deployed.
- **Window size**: the module starts the swapchain at a default size and relies on `GetSurfaceSize()`/`Resize` for the real size on the first out-of-date.

## Migration Plan

Additive. Default renderer stays `SkyRender`; `AuroraRender` is opt-in via `--renderer aurora`. Rollback = don't pass the flag.

## Open Questions

- Should `AuroraModule` own the window (create `NativeWindow`) instead of using `Platform::GetMainWinHandle()`? (Current: reuse the platform main window.)
- Where does the real `Renderer` (RDG scene) attach — inside `AuroraModule::Tick` or a separate renderer object? (Current: `Tick` is the clear loop only.)
