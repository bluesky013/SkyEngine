## Why

The desktop editor is a legacy Qt5 Widgets application whose shell (`MainWindow`, `QDockWidget` panels, property
grid, asset browser) is deeply Qt-bound, and whose viewport still renders through the legacy `RenderCore` path
(`Renderer::CreateRenderWindow`) instead of Aurora. The editor is only ever used on macOS and Windows, so Qt's
cross-platform reach buys little, while it drags a large external dependency, LGPL/commercial licensing,
`windeployqt` deployment weight, and a second rendering stack into an engine that is explicitly Aurora-first. At
the same time the engine already owns most of the ingredients for a self-hosted editor: an in-house retained UI
core (`engine/ui`), a native-window present path (`SwapChain::Descriptor.window` + `ClientViewport` for
HWND/`CAMetalLayer`), and a non-Qt launcher. Editor logic is also entangled with Qt today (no editor-wide
undo/redo, property editing embedded in Qt widgets), so the core editor services must be separated from the UI
toolkit before, not during, a shell rewrite. This change decides the shell/UI architecture, the rendering/
windowing model, and the decoupled core services, then rebuilds the editor **Qt-free** in `engine/sandbox`; the
legacy Qt editor (`engine/editor`) is left untouched and unused.

## What Changes

- Establish a toolkit-independent editor shell: native application layer (window, main loop, file dialogs,
  clipboard, IME) for Win32 and Cocoa, with all editor panels — and the menu bar — drawn by the engine.
- Fix the rendering/windowing model: the editor renders every frame through Aurora into **one main native window
  and one swapchain**; the editor UI pass and the viewport are composited in the same frame. The legacy
  per-viewport child window (`Renderer::CreateRenderWindow`) is retired from the editor; the native handle is
  supplied through `SwapChain::Descriptor.window`. Extra swapchains only for explicitly detached windows.
- Decouple the core editor services from the UI toolkit, so they are headless-testable and view-independent:
  - undo/redo (command/transaction stack, reflection-driven property commands),
  - the reflection property model (descriptors over `serialize::TypeMemberNode`, with `Get`/`Set`/metadata),
  - documents/asset editing lifecycle (open/load/save/dirty),
  - selection and editor context.
  Panels are views over these services.
- Establish the module layout under `engine/sandbox`: `core` (`EditorCore`, dependency-guarded, builds with or
  without Qt), `src` (`Sandbox`, opt-in via `SKY_BUILD_SANDBOX`, non-Qt shell prototype), and `test`
  (`EditorCoreTest`, via `SKY_BUILD_TEST`). The legacy Qt editor `engine/editor` is deprecated and is not modified.
- Bootstrap Phase 1 by **borrowing the existing native window framework** (`Framework` `Platform` +
  `NativeWindow`, SDL-backed Win32/Cocoa) for early validation, rather than writing new Win32/Cocoa backends
  first; the sandbox opens a non-Qt window and stays **native-window-only with no Aurora/RHI dependency**.
  Aurora present is introduced later as a separate RHI-linked host.
- Adopt **Scheme B** (decided in `design.md`): panels are `sky::ui` trees rendered by a new Aurora UI pass, driven
  by a thin native shell. Define the panel framework and how panels (world outliner, inspector/property grid,
  asset browser, output log, console, asset editors, viewport) are authored on it.
- Manage the editor layout as a toolkit-independent model (`editor-layout`): a split/tab/panel tree with a panel
  registry, JSON save/restore with a version, and reset-to-default; the shell renders it with draggable splitters
  and tab bars inside the single main window (no child OS windows).
- Provide an editor Output Log and command-line console (`editor-console`) built on the existing
  toolkit-independent console subsystem (`CommandShell`, `CommandRegistry`, `ConsoleLog`, `CommandHistory`,
  `IConsoleUI`): a `sky::ui` panel renders the transcript and input line, the logger is piped through
  `ConsoleLog` and drained on the main thread, and commands come from `CommandShell`.
- Confine ImGui to runtime/debug data editing (console, data inspectors, gizmo overlays); it SHALL NOT define the
  editor shell.
- Retire Qt from `engine/editor`; re-home the Aurora editor extension registration (`AuroraRender.Editor`) onto
  `EditorCore`. Legacy render extensions (`SkyRender.Editor`, PVS.Editor) are left unmodified and retire with the
  legacy render stack.

**Non-goals**: Linux support; a web/CEF editor; building virtualized multi-million-row content views or a
scripting IDE; adding editor features unrelated to the shell migration.

## Capabilities

### New Capabilities
- `editor-core`: the render- and toolkit-independent editor core module boundary (its link set and the
  configure-time guard forbidding `aurora/`, `render/`, `ui/`, and Qt includes), hosting undo/redo, the property
  model, documents, and selection.
- `editor-application`: editor process lifecycle, native application shell (window/event loop/menus/dialogs/IME)
  for Windows and macOS, module loading, and the frame/tick driver independent of any UI toolkit.
- `editor-ui-shell`: the editor panel framework — panel hosting, menus/toolbars, theming, and focus/input routing
  between panels and the viewport.
- `editor-layout`: a toolkit-independent layout model (split/tab/panel tree) with a panel registry, layout
  operations, and versioned JSON save/restore, rendered by the editor shell.
- `editor-console`: an editor Output Log / command-line panel built on the existing toolkit-independent console
  subsystem (`CommandShell`/`CommandRegistry`/`ConsoleLog`/`CommandHistory`/`IConsoleUI`), viewed through
  `sky::ui` and independent of any UI toolkit.
- `editor-viewport`: an Aurora-backed editor viewport composited into the main window's swapchain, owning
  `ClientViewport` creation from the native handle, resize/present, and editor camera/gizmo/profiler overlays.
- `editor-undo-redo`: a UI-toolkit-independent command/transaction service with `Execute`/`Undo`/`Redo` and
  transaction grouping, including a reflection-driven generic property-edit command.
- `editor-property-model`: a UI-toolkit-independent reflection property model (descriptors, `Get`/`Set`,
  metadata, child descriptors) that inspector/asset-editor panels render as views.
- `editor-document`: the document/asset editing lifecycle (open, load, save, dirty state, asset-to-document
  mapping), independent of the UI toolkit.
- `editor-selection`: the observable editor selection (world/entities/assets) and active editor context that the
  outliner, inspector, and viewport subscribe to.

### Modified Capabilities
<!-- No existing spec-level requirements change yet; the UI route decision is pending. -->

## Impact

- `engine/sandbox/**` (new: `core` = `EditorCore` + configure-time dependency guard, `src` = `Sandbox` shell
  prototype, `test` = `EditorCoreTest`); `cmake/options.cmake` (`SKY_BUILD_SANDBOX`), `engine/CMakeLists.txt`.
- `engine/editor/**` (legacy Qt editor): deprecated and **not modified** in this change.
- `engine/framework` platform layer (native window shell, file dialogs, clipboard, text input/IME for Win32 and
  Cocoa; the editor menu bar itself is engine-drawn).
- `engine/ui/render` (Aurora UI pass, currently a placeholder TU); `engine/ui/core` is used as-is (the docking/
  layout model lives in `EditorCore`, per `editor-layout`).
- `engine/aurora/adaptor` (`ClientViewport` host integration, single-swapchain compositing) and
  `engine/aurora/editor`.
- `engine/aurora/editor` extension registration moves onto `EditorCore` when the sandbox editor loads modules
  (deferred); legacy `engine/render/editor` and `plugins/pvs/editor` are left unmodified.
- Build/deploy: the sandbox targets link no Qt; no Qt build changes are made to the legacy `engine/editor`.
- Licensing/dependency surface: the rebuilt editor has no Qt dependency; the deprecated Qt editor is left as-is.
