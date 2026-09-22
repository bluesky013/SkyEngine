## Why

The archived `editor-shell-redesign` scaffolded `engine/sandbox/render` (`EditorRender`) but it only logs a
placeholder — nothing is drawn. The editor should be hosted by the engine's own application/module system (the
`launcher` + `ModuleManager` path, as `AuroraRender` is), not by a bespoke shell, so it participates in module
loading, deploy dependencies, and the engine frame tick. This change adds an `EditorModule` that owns the editor
frame and implements the editor renderer: clear/present end to end, then the GUI pipeline over `sky::ui` draw
data. It also supports standalone preview windows.

## What Changes

- Host the editor through the engine application/module system: a non-Qt launcher-style host using
  `Application`/`GameApplication` + `ModuleManager` loads `configs/modules_editor.json` (rewritten for the new
  modules). `GameApplication` gains a config override so the editor can select `modules_editor.json`.
- Add **`SandboxModule`** (`IModule`, SHARED, `REGISTER_MODULE`, mirroring `AuroraModule`/`AuroraRender`) in
  `engine/sandbox`: owns the editor frame — initializes the Aurora device + frame context + command pool, creates
  the main-window `ClientViewport`, drives the GUI pipeline through `EditorRender`, with
  `Init`/`Start`/`Tick`/`Shutdown`. The existing sandbox shell (`engine/sandbox/src`) is **kept** as the
  RHI-free development shell; the module is the production host path.
- Implement the **GUI pipeline** in `engine/ui/render` (`UIRender`, owned by the engine so the runtime UI and the
  editor share one renderer): UI shaders/PSOs/batching, clip/scissor, consuming `sky::ui` `UIDrawData`;
  `EditorRender` consumes `UIRender` and draws a visible quad as the milestone gate.
- Support **multiple previews** with two presentation modes, decoupled from the preview content:
  - **Embedded**: the preview renders to an offscreen render target and is composited as an image element in a
    main-window panel by the GUI pipeline (single swapchain).
  - **Standalone**: the preview renders to its own `NativeWindow` + `ClientViewport` swapchain.
  Previews can be created/destroyed and **switched between embedded and standalone at runtime**; all use the
  single Aurora device. Preview content may be a placeholder until the engine scene renderer
  (`aurora-renderer`) lands.
- Register the RHI module deploy dependencies; the sandbox shell stays RHI-free.
- Out of scope: the engine scene pipeline / 3D scene content (`aurora-renderer`), PIE, docking/panels, ImGui.

## Capabilities

### New Capabilities
- `editor-render`: the editor-owned renderer — initialization on the Aurora device, the clear/present frame loop,
  and the GUI pipeline drawing `sky::ui` draw data (quads/images, batching, clip), independent of the engine
  scene pipeline.
- `editor-preview`: multiple previews, decoupled into a content target plus a presentation that is either
  **embedded** (composited as a UI image in the main window) or **standalone** (its own native window +
  swapchain), with runtime mode switching on the single Aurora device.

### Modified Capabilities
<!-- None: `editor-application` already specifies module loading and the frame driver. -->

## Impact

- `engine/ui/render` (`UIRender`: the GUI pipeline — UI shaders, PSOs, batching, the UI pass) is implemented here
  and shared by the runtime and the editor.
- `engine/sandbox/render` (`EditorRender`: the editor frame/host + previews; consumes `UIRender`).
- `engine/sandbox` new module target (SHARED `SandboxModule`, `REGISTER_MODULE`) hosted by the launcher-style app;
  the existing sandbox shell (`engine/sandbox/src`) is retained as the RHI-free dev shell.
- `engine/framework/application/GameApplication` (config override for the editor module config).
- `engine/configs/modules_editor.json` (rewritten: `SandboxModule` + required modules, dropping legacy
  `SkyRender.Editor` entries).
- Aurora RHI / `ClientViewport` used as-is; additional `NativeWindow` + `ClientViewport` for preview windows.
- Module deploy dependencies (`sky_add_dependency(... DEPENDENCIES ...)`) for the editor host.
