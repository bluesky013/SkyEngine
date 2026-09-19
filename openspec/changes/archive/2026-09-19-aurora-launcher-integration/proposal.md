## Why

Aurora already has RHI (Instance/Device/SwapChain), `ClientViewport`, frame context, RDG and a command-buffer clear path, but nothing loads it from the launcher. The launcher boots through framework `GameApplication` → `ModuleManager`, which dynamically loads a render module (the old stack is `SkyRender`, a `SHARED` dll with `REGISTER_MODULE`). Aurora has no module, no window/surface wiring, and no frame loop, so the launcher cannot drive it.

## What Changes

- New **`AuroraRender`** dynamic module under `engine/aurora/adaptor/` implementing `sky::IModule` (+ `REGISTER_MODULE`):
  - `Init`: parse RHI API from args, create `aurora::Instance` + `Device` + `DeviceFrameContext`.
  - `Start`: obtain the native window handle from the platform, create `ClientViewport` (SwapChain + sync semaphores).
  - `Tick`: minimal "can present a frame" loop — frame begin → viewport `Begin`/`Acquire` → backbuffer layout barrier → `BeginRendering` clear → submit (wait acquire sema, signal render-done sema, frame fence) → `Release`/present → frame end.
  - `Shutdown`: reverse teardown.
- **Remove legacy render from the launcher flow**: `GameApplication` registers only `AuroraRender` (no `--renderer` switch, no `SkyRender`); `Launcher` drops the legacy `RenderAdaptor` link.
- **Window handle from the host**: add `ISystemNotify::GetMainWindowHandle()` (implemented by `GameApplication` from its `NativeWindow`); the module uses it instead of the Windows SDL `Platform::GetMainWinHandle()` (which returns null).
- **Engine builtin configs**: ship `engine/configs/` and deploy it next to the executable as `configs/`; `GameApplication` falls back to it when the project has no configs, so the launcher boots without an external project.
- **Build**: `AuroraRender` shared target linked against `Aurora.RHI` + `Framework`, with a dependency so the dll and the backend dlls are deployed next to the executable.
- Safety: uninitialized/no-window and acquire-failure paths are no-ops (no crash, no present).

## Capabilities

### New Capabilities
- `aurora-launcher`: the launcher/module integration contract — how the AuroraRender module is discovered, initialized, driven per tick, and torn down.

### Modified Capabilities
- (none)

## Impact

- New: `engine/aurora/adaptor/` (module header/source/registry/CMake), `engine/configs/` (builtin configs).
- Modified: `engine/aurora/CMakeLists.txt` (add subdirectory); `engine/framework/.../GameApplication.{h,cpp}` (load only `AuroraRender`, builtin-config fallback, main-window handle); `engine/framework/.../ISystem.h` (window handle); `engine/launcher/CMakeLists.txt` (drop `RenderAdaptor`, deploy configs).
- No change to RHI/backends. Old `SkyRender` path unchanged (default).
- First milestone is intentionally minimal (no Renderer/RDG scene yet); it validates module discovery, window/surface creation, submit/present and resize.
