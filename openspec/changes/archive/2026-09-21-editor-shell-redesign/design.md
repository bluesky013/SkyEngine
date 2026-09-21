## Context

The editor is the only Qt application in the tree (`engine/editor/CMakeLists.txt:3` guards the whole target behind
`find_package(Qt5 COMPONENTS Widgets)`). It uses **only `Qt5::Widgets`** — no QML/Quick/WebEngine/Qt3D.
~37 of 57 editor source files include Qt directly, and the coupling is concentrated in the shell:
`src/window/*` (`QMainWindow`/`QMenuBar`/`QToolBar`), `src/dockwidget/*` (`QDockWidget`), `framework/*Widget*`
(`QFileSystemModel`, `QTreeView`, `QListWidget`, drag/drop), and `ReflectedObjectWidget.h` (a 500+ line
reflection-driven property grid built from `QLineEdit`/`QComboBox`/`QColorDialog`/`QFileDialog`). The viewport is
a `QWindow` + `createWindowContainer` rendering through the **legacy** stack
(`Renderer::Get()->CreateRenderWindow(...)`, `ViewportWidget.cpp:367`), not Aurora. There is no editor-wide
undo/redo (only the UI-document editor `UIDocumentEditor` has snapshot undo/redo) and no UI-toolkit-independent
property model. The editor targets **macOS + Windows only**.

Existing building blocks that change the cost calculus:

```
┌──────────────────────────────────────────────────────────────────────────┐
│  1. engine/launcher + GameApplication  → non-Qt app shell, loads          │
│     AuroraRender only (Aurora-first).                                     │
│  2. Framework Platform + NativeWindow   → SDL-backed Win32/Cocoa window,   │
│     GetNativeHandle() = HWND / NSView. Aurora takes the same handle:      │
│     SwapChain::Descriptor.window, owned by ClientViewport.                │
│  3. engine/ui/core (sky::ui)            → retained UI core: element tree,  │
│     layout, style/theme, 2D transform, text/font atlas, widgets, JSON      │
│     documents, animation, localization, event routing. Has tests.          │
│     engine/ui/render is a PLACEHOLDER TU (Aurora UI pass not implemented).  │
└──────────────────────────────────────────────────────────────────────────┘
```

`AuroraRender.Editor` is already toolkit-agnostic (links only `Aurora.Adaptor` + `Core`); `SkyRender.Editor` is
Qt-bound. ImGui (`engine/render/imgui`, v1.88, **non-docking**) rides the legacy render path and is used only for
the runtime console, the `plugins/guizmo` overlay, and dev tools.

This design (1) states the overall architecture, (2) fixes the rendering/windowing model, (3) separates the core
editor services from any UI toolkit, (4) compares how established editors are built, and (5) commits to UI route
**B**. The legacy Qt editor (`engine/editor`) is deprecated and is **not modified**; the new editor is rebuilt
from scratch under `engine/sandbox` (`core` = toolkit/render-independent services, `src` = non-Qt shell,
`test` = headless tests).

## Goals / Non-Goals

**Goals:**
- A self-hosted editor: a thin native platform layer owns window/event loop/native menus/file dialogs/clipboard/
  IME, and all editor panels are drawn by the engine (RHI).
- A Qt-free editor from the start: the rebuild under `engine/sandbox` never links Qt (the legacy Qt editor
  `engine/editor` is untouched and unused), and runs on the engine's own rendering/UI stack (Aurora-first).
- Decouple core editor services (undo/redo, property model, documents, selection, module registration) from the
  UI toolkit, so they are headless-testable and survive any shell rewrite.
- Reuse existing investment (`sky::ui`, `Platform`/`NativeWindow`, `ClientViewport`) where it lowers cost.
- Keep ImGui scoped to runtime/debug data editing, never the editor shell.
- A phased migration that keeps working software at each step.

**Non-Goals:**
- Linux/other desktop support; a web/CEF-based editor.
- Pixel-perfect Qt/OS-native look; OS-native widget panels.
- Editor features unrelated to the shell (workflow/tooling redesign).
- Virtualizing content browsers at extreme scale, or a full scripting IDE.

## Overall Architecture

Everything above the native layer is drawn by the engine; the OS provides only shell services. This mirrors how
UE, Blender, and Godot are structured (see Prior Art).

```
┌───────────────────────────────────────────────────────────────────────┐
│ Editor panels (sky::ui / editor UI framework)                          │
│   world outliner | inspector/property grid | asset browser | console   │
│   | asset editors | viewport                                           │
├───────────────────────────────────────────────────────────────────────┤
│ Editor shell: docking/layout, menus/toolbars, theming, focus & input   │
│   routing between panels and the viewport                              │
├───────────────────────────────────────────────────────────────────────┤
│ EditorCore (toolkit- AND render-independent)                           │
│   undo/redo | property model | documents | selection | module registry │
├───────────────────────────────────────────────────────────────────────┤
│ UI framework: engine/ui (sky::ui)  +  Aurora UI pass (engine/ui/render)│
│   element tree, layout, style, text, draw data → RHI                    │
├───────────────────────────────────────────────────────────────────────┤
│ Rendering: Aurora RHI + ClientViewport                                  │
│   one swapchain into the main native window                            │
├───────────────────────────────────────────────────────────────────────┤
│ Native shell: Platform + NativeWindow (Win32/Cocoa, SDL-backed)        │
│   window, event loop, native menus, file dialogs, clipboard, IME       │
└───────────────────────────────────────────────────────────────────────┘
```

Module layout:

| Path | Target | Depends on | Role |
|---|---|---|---|
| `engine/framework` | `Framework` | `Core` | platform/window, reflection, world/ECS, assets |
| `engine/ui/core` | `UI` | `Core`, `Framework` | retained UI core (`sky::ui`), render-agnostic (guarded) |
| `engine/ui/render` | `UIRender` | `UI`, Aurora | UI pass → Aurora (currently a placeholder) |
| `engine/aurora/**` | Aurora RHI/Pipeline | — | rendering backends (Vulkan/DX12/Metal) |
| `engine/sandbox/core` | `EditorCore` | `Core`, `Framework` | decoupled editor services (guarded) |
| `engine/sandbox/render` | `EditorRender` | `UI`, `Aurora.RHI`, `Aurora.Pipeline` | editor-owned renderer (GUI pipeline + presentation), independent of the scene pipeline |
| `engine/sandbox/src` | `Sandbox` | `EditorCore`, `Framework` | opt-in shell prototype, **no Qt / no RHI** |
| `engine/sandbox/test` | `EditorCoreTest` | `EditorCore`, googletest | headless core-service tests |
| `engine/editor` | `Editor` | Qt | **legacy, unused** Qt editor (not modified) |

Principles:
1. **Native layer never draws UI.** Window/input/OS services only.
2. **EditorCore never sees a toolkit or the RHI.** Enforced by a configure-time guard.
3. **Panels are views over core services**, never the other way around.
4. **The viewport shares the editor's device and frame**, not a child OS window.

## Rendering & Windowing Model

Rendering and windowing are layers, not alternatives: **Aurora RHI produces every frame; the native window is
only the presentation surface and input source.** The engine boundary already exists —
`SwapChain::Descriptor.window` (`HWND` / `CAMetalLayer*` / `ANativeWindow*`) owned by `ClientViewport`. The
design question is how many swapchains exist and how editor UI + viewport are composited:

```
(1) Single main window, single swapchain            ← recommended target
    native window ──▶ one Aurora swapchain ──▶ frame:
      • editor UI pass paints panels/menus (sky::ui)
      • viewport rendered to an offscreen target, composited as a UI image,
        or drawn directly into a scissored region of the same backbuffer
    Pros: one present path, correct z-order, no child-window/DPI issues,
          editor UI and viewport share the same frame and resources.
    Cons: the editor UI layer owns viewport input routing and resize.

(2) Main window + per-viewport child native window     ← current Qt model, to retire
    each viewport owns its own swapchain (Renderer::CreateRenderWindow today)
    Pros: input handling is simple.
    Cons: multiple swapchains, z-order/DPI/compositing problems, toolkit-coupled.

(3) Hybrid / multi-window
    main window keeps (1); detached/floating panels or viewports get their own
    native window + swapchain on demand.
```

Decision input: adopt (1) by default, (3) only for explicitly detached windows. Phase 1 does not need any
rendering at all — the shell is validated on a native window with **no RHI dependency**.

### Device and targets

One RHI device per process. Multiple render targets, multiple worlds, and multiple swapchains are all recorded
with that single device; multiple devices in one process are possible but unnecessary and avoided (duplicated
allocators/queues, hard resource sharing). This matches how Godot/UE/Blender/Unity operate: **one device, many
targets** — not one device per viewport, and not a second device for the editor.

### Editor renderer (independent render flow)

The editor owns its own renderer (`EditorRender`) that records its own passes on the Aurora device and presents
to the editor swapchain. It implements the **GUI pipeline** (`sky::ui` draw data -> UI shaders/PSOs/batching)
plus the UI render-graph pass, and is independent of the engine scene pipeline: a *UI pass* is the render-graph
node, the *GUI pipeline* is the PSO/batching family it records with. Consequences:

- Editor UI bring-up does **not** depend on the engine top-level renderer (`aurora-renderer`, still a TODO); the
  GUI pipeline only needs the Aurora device + swapchain.
- The 3D viewport is a later composition step: the engine scene renderer renders the edited world into an
  offscreen target and the editor renderer composites it as a UI image (this is also where a process-isolated PIE
  game would hand over a shared texture). Until `aurora-renderer` lands the viewport can render a placeholder
  (clear color / test pattern).
- Optional debug overlays (for example ImGui) are additional passes in the same editor renderer.

## Core Services (Decoupled from the UI Shell)

These services are editor capabilities whose contracts must not mention a UI toolkit. Panels are *views* over
them. They live in `engine/sandbox/core` (target `EditorCore`), which links only `Core` + `Framework` and is
guarded at configure time against `aurora/`, `render/`, `ui/`, and Qt includes.

### 1. Undo / Redo (回退 / 恢复)
- **Contract**: a UI-agnostic command/transaction service. An undoable command has `Do`/`Undo`; a transaction
  groups commands into one user-visible step: `Execute`, `Undo`, `Redo`, `BeginTransaction`/`EndTransaction`,
  `CanUndo`/`CanRedo`, a change notification, and a scope (per-document stack or global with document tagging).
- **Reflection integration**: property edits are recorded by a generic `PropertyEditCommand` capturing
  (object handle, member path, old value, new value) generated from the reflection model — no per-type code.
- **Snapshot vs command**: commands for fine-grained edits, snapshots for coarse operations (world/asset
  serialization), reusing the existing snapshot idea from `UIDocumentEditor`.
- **Current state**: only `UIDocumentEditor` has snapshot undo/redo; `ActionManager` is Qt-only and unconnected.

### 2. Property Model (反射面板)
- **Contract**: a UI-toolkit-independent descriptor layer over the C++ reflection data
  (`serialize::TypeMemberNode`). A `PropertyDescriptor` exposes name/display name/category/type/metadata,
  `Get()`/`Set()` against an object handle, validators, and child descriptors (vectors/structs/arrays/maps).
  Value edits route through the undo service.
- **Views**: the inspector/property grid is a view over descriptors. The existing Qt implementation
  (`ReflectedObjectWidget.h`: `PropertyScalar`/`PropertyVec`/`PropertyColor`/`PropertyEnum`/
  `PropertySequenceContainerWidget`) is the parity target and can be re-hosted on `sky::ui` unchanged in spirit.
- **Current state**: logic and Qt widgets are entangled in `ReflectedObjectWidget`; no standalone model exists.

### 3. Documents & Asset Editing
- **Contract**: document/asset editing lifecycle — open, load, save, dirty state, asset-to-document mapping.
- **Current state**: `editor/document/Document.h` is thin and Qt-typed (`QString`/`QFile`); asset editors are
  registered through `AssetCreator`.

### 4. Selection & Editor Context
- **Contract**: the current selection (world/entities/assets) and active editor context as an observable service;
  the outliner, inspector, and viewport subscribe.
- **Current state**: selection is passed ad hoc between Qt widgets.

### 5. Module / Extension Registration
- **Contract**: editor extensions register asset creators, actor creators, gizmo factories, and inspectors
  through a toolkit-agnostic API.
- **Current state**: `AuroraRender.Editor` conforms and now links `EditorCore`; `SkyRender.Editor` does not (links
  `EditorFramework` + Qt) and is left unmodified — it retires with the legacy render stack.

## Editor Layout Management

Panels (outliner, inspector, content browser, output log, console, viewport) are arranged by the editor shell.
**Layout is a data model; the shell renders and interacts with it.** `sky::ui` provides only per-element anchored
placement (`UILayoutParams` / `ComputeElementBounds`), so docking is built in the editor layer, not the UI core.

```
LayoutNode
 |- SplitNode  { orientation: Horizontal | Vertical, ratios: [f32] }
 |    |- TabNode { panels: [PanelNode], activeIndex }
 |    |    `- PanelNode { panelId: string }
 |    `- TabNode { ... }
 `- TabNode { ... }

PanelRegistry: panelId -> { title, icon, minSize, create() }
```

- **Model (toolkit- and render-independent, headless-testable; lives in `EditorCore`)**: `SplitNode`/`TabNode`/
  `PanelNode` plus a `PanelRegistry`. Operations: split, move-to-area, tabify, close, set-ratio, reset-to-default.
- **Persistence**: JSON with a `version` field (follows the existing JSON-archive style used by
  `UIDocument`/`UIDocumentEditor`); stored per-user and restored on startup. Unknown or missing panel ids are
  skipped so an old layout never breaks startup. Multiple named **workspaces/profiles** are a possible extension.
- **Interaction (editor shell, `sky::ui`)**: the shell computes each area/panel rect from the model and sets the
  panel's bounds directly (no container layout is required from `sky::ui`); splitters drag to change ratios; the
  tab bar activates/reorders tabs and drag-to-tabify re-parents panels; each panel has a title bar and close.
- **Windowing**: panels live inside the single main window (topology (1)); floating/detached panels
  (topology (3)) are optional and deferred.
- **Current state**: `sky::ui` has no docking/tab/split; the Qt editor has a `DockManager` but no layout
  persistence (`saveState`/`restoreState` are unused), so this is net-new with no parity burden.

## Process Model

The editor and the running game must **not** share a process. The engine is built on process-wide singletons
(`Singleton<T>`: `Renderer`, `AssetManager`, `ModuleManager`, `CommandRegistry`, `AssetCreatorManager`,
`EditorActorCreation`, ...), a single global logger callback, and one RHI device; two engine instances in one
process would collide on all of them. Crash isolation is the second reason.

- **Editor: one process.** Native window, panel UI, `EditorCore` services; no game runtime.
- **Play / PIE: a separate child process.** The editor launches a game process and talks to it over IPC; the game
  owns its own runtime state and (initially) its own window/swapchain.
- **Cook / import: worker processes** for heavy or crash-prone offline work.
- **Modules/extensions: in-process** (loaded via `ModuleManager`); they are not isolated.

```
┌─────────────────────────────┐        IPC          ┌─────────────────────────────┐
│ Editor process              │  named pipe / unix  │ Game process (PIE)          │
│  window + sky::ui panels    │◀───── socket ──────▶│  Aurora window + swapchain  │
│  EditorCore (PlaySession)   │  control + log +    │  modules_game + scene       │
│  editor renderer            │  stats + pick       │  game renderer              │
└─────────────────────────────┘                     └─────────────────────────────┘
```

PIE rendering modes:

1. **Separate window** (first): the game process owns an OS window and swapchain (topology (2)); the editor shows
   session state only. Lowest cost.
2. **Shared texture into the editor viewport** (later): the game renders offscreen and exports a GPU-shared
   texture handle (DX12 shared heap / Metal `IOSurface`) that the editor imports and composites into the viewport
   panel (topology (1)). Best UX; platform-specific.
3. **Headless** (CI/automation): no window; the game process runs logic only and reports over IPC.

Lifecycle: editor `Start` spawns the child with `--pie --ipc <name> --scene <path>`; the game connects and reports
`Ready`; the editor sends control/input and receives log/stats/pick; `Stop` sends a graceful shutdown and kills
after a timeout. On crash (IPC drop or process exit) the editor marks the session crashed, reaps the process, and
surfaces the forwarded log.

Requirements so the separation stays possible:
- Editor core services stay instance-based (they are: `CommandService`, `LayoutModel`, `LogService`, ...); do not
  assume a single global runtime.
- Add a `Process`/launch abstraction and an `IpcChannel` to the platform layer.
- The game forwards its log to the editor so there is a single console.

## Prior Art: How Other Editors Are Built

A survey of established editors shows an overwhelming pattern: **the editor is drawn by the engine's own
renderer, and the OS is used only as a platform/shell layer.**

| Aspect | UE (UnrealEd / Slate) | Blender | Godot | Unity | O3DE / Maya / Houdini |
|---|---|---|---|---|---|
| Platform layer | `GenericApplication` (Win32/Cocoa/X11) | **GHOST** (win32/cocoa/x11/wayland/headless) | **DisplayServer** | native OS layer | Qt |
| Editor UI tech | **retained** Slate widgets (`SWidget`, `SCompoundWidget`, `SNew`, `TAttribute`) | custom C UI in `editors/interface`, **immediate-mode** layout (`uiLayout`/`uiBlock`/`uiBut`) | **retained** `Control` nodes on the scene tree | **IMGUI** (immediate) + **UI Toolkit** (retained, UXML/USS) | Qt widgets |
| Drawn by | own RHI (Slate batches draw elements) | own GPU module | own renderer | own | Qt / OpenGL |
| Docking | `FTabManager`/`FGlobalTabmanager`, in-engine tabs/splits, floating Slate windows | WM screens/areas/regions, split/join, saved in workspace files | `DockableContainer`/`TabContainer` | custom | Qt `QDockWidget` |
| Property panels | reflection-driven `IDetailsView` + `IDetailCustomization` | RNA + `uiLayout`, reflection-driven | `_get_property_list` | custom / IMGUI | Qt forms |
| Viewport | `SEditorViewport`/`FSceneViewport` (Slate widget) | 3D-view region | `SubViewport` in the editor scene | native child window | Qt native child |
| Runtime UI reuse | UMG built on Slate | same interface system | same Control system | UI Toolkit | n/a |
| Dependency / license | in-house, source-available | GPL, in-house | MIT, in-house | proprietary | Qt (LGPL/commercial) |

Distilled lessons:

1. **Native OS is a shell, not the UI.** UE `GenericApplication`, Blender **GHOST**, and Godot `DisplayServer`
   all abstract windowing/input/IME/clipboard, and the editor panels are engine-drawn. Qt-based editors
   (O3DE/Maya/Houdini) exist but are the minority and pay a dependency/style cost.
2. **Both retained and immediate-mode in-engine UI scale.** UE Slate and Godot `Control` are retained; Blender's
   interface and Unity IMGUI are immediate-mode and still carry DCC-scale editors. Retained better fits rich
   panels, data binding, and this repo's existing `sky::ui` investment; immediate-mode fits debug/data tools
   (which is where ImGui already sits here).
3. **Docking is always in-engine.** `FTabManager`, Blender areas/regions, Godot `DockableContainer` — never OS
   dock windows. Expect to build it (or adopt a minimal model) whichever route is chosen.
4. **Reflection-driven property panels are standard.** UE's `IDetailsView`, Blender's RNA-driven `uiLayout`,
   Godot's `_get_property_list` all separate "what properties exist" (model) from "how they are drawn" (view) —
   exactly the decoupling proposed for `editor-property-model`.
5. **The platform abstraction is the durable piece.** GHOST/`GenericApplication`/`DisplayServer` map directly to
   `Framework`'s `Platform` + `NativeWindow`; Phase 1 borrows it rather than rewriting it.
6. **The UI shares the engine's render device.** UE Slate on RHI, Godot's renderer, Blender's GPU module — all
   confirm a single-device, single-swapchain topology (model (1)).

## Candidate Schemes

All candidates are judged against the architecture above: self-hosted (RHI-drawn UI, native shell only) and
toolkit-independent core services.

```
   Current (Qt)                          Target (self-hosted)
 ┌───────────────────────────┐       ┌───────────────────────────┐
 │ Qt widgets / dock widgets │       │ Editor panels (sky::ui)   │
 ├───────────────────────────┤       ├───────────────────────────┤
 │ Qt event loop / models    │       │ Editor UI framework       │
 ├───────────────────────────┤       ├───────────────────────────┤
 │ legacy RenderCore         │       │ Aurora UI pass            │
 │ + QWindow child viewport  │       │ (engine/ui/render)        │
 ├───────────────────────────┤       ├───────────────────────────┤
 │ Win32 / Cocoa (via Qt)    │       │ Native shell (Win32/Cocoa)│
 │                           │       │ + ClientViewport          │
 └───────────────────────────┘       └───────────────────────────┘
```

### Scheme A — New in-house retained UI framework on Aurora
Write a new retained-mode UI framework and render it with a new Aurora pass; native only for shell services.
- **Pros**: tailored to the editor; one UI stack for editor and runtime; no external dependency/licensing.
- **Cons**: largest effort; duplicates `engine/ui`; requires the Aurora top-level renderer and a UI pass first.
- **Prior art**: essentially rebuilding Slate or Godot's Control system. Valid but wasteful here.
- **Verdict**: strategically clean but duplicates `sky::ui` → prefer B.

### Scheme A2 — Immediate-mode in-engine editor UI (Blender / Unity IMGUI style)
Author panels as per-frame immediate-mode code drawn by the engine, rather than a persistent widget tree.
- **Pros**: less state and boilerplate; Blender proves it scales; no widget-tree lifetime management.
- **Cons**: weak for rich text editing, docking, and data binding; would not reuse the retained `sky::ui` core;
  the user has scoped ImGui to runtime/debug data editing only.
- **Prior art**: Blender's `editors/interface`, Unity's classic IMGUI.
- **Verdict**: rejected for the editor shell; keep immediate-mode for runtime/debug tools.

### Scheme B — Reuse `engine/ui` (`sky::ui`) as the UI framework
Extend the existing retained core with the missing shell pieces (docking, property grid, tree/table, text
editing/IME) and implement the `engine/ui/render` Aurora pass; editor panels are `sky::ui` trees over the core
services.
- **Pros**: leverages existing investment and tests; render-agnostic core already enforced; can drive runtime UI
  too (Godot-style dogfooding); smaller than A; matches the retained paradigm proven by UE and Godot.
- **Cons**: must add docking, property grid, tree/table virtualization, text editing + IME; must finish the
  Aurora renderer + UI pass; retained panels are more code than immediate-mode.
- **Verdict**: recommended target.

### Scheme C — Pragmatic: Qt6 shell + Aurora viewport
Upgrade Qt5→Qt6, keep mature widgets/docking/IME/accessibility, swap only the viewport to Aurora `ClientViewport`.
- **Pros**: lowest effort and risk; fastest path to a shippable editor; can be an intermediate stage toward B.
- **Cons**: large external dependency + licensing + deployment weight; second UI style; no editor/runtime UI
  unification; viewport is an embedded native child window (topology (2)).
- **Prior art**: O3DE/Maya/Houdini — viable, but a stylistic and dependency outlier.
- **Verdict**: best *transition*, not the strategic end state.

### Scheme D — Thin native shell + OS-native widget panels
Build panels with Win32/Cocoa widgets.
- **Rejected**: two non-portable widget codebases, no editor-grade docking, no style unification — strictly worse
  than C, and no surveyed editor does this.

### Scheme E — Rust + WinUI (or Rust + egui)
- **Rejected**: WinUI 3 is Windows-only (macOS would need a separate AppKit UI); it adds a second language and
  build system (Cargo) plus a C-ABI bridge across the whole C++ editor surface; WinUI `SwapChainPanel` binds to
  DirectX and conflicts with Aurora's multi-backend `ClientViewport`. Rust+egui is cross-platform but is still a
  second language and does not unify with engine UI.

### Scheme F — Web/CEF editor shell
- **Rejected**: large binary/runtime, input-latency and GPU-interop friction with the RHI, no UI unification.

### Comparison matrix

| Dimension | A: new retained UI | A2: immediate-mode | B: reuse `sky::ui` | C: Qt6 + Aurora viewport | D: native widgets | E: Rust+WinUI |
|---|---|---|---|---|---|---|
| UI paradigm | retained | immediate | retained | OS/Qt widgets | OS widgets | WinUI widgets |
| Windows + macOS | yes | yes | yes | yes | yes (x2 code) | Windows only |
| Windowing topology | (1) | (1) | (1) | (2) | (2) | WinUI DX swapchain |
| Docking source | build | build | build (missing) | Qt built-in | build | build |
| Editor↔runtime UI unified | yes | yes | yes | no | no | no |
| Core services toolkit-independent | yes | yes | yes | no | no | no |
| Reuses `sky::ui` | no | no | yes | no | no | no |
| Prior-art precedent | UE/Godot | Blender/Unity | UE/Godot | O3DE/Maya | none | none |
| Effort | highest | high | high | lowest | high | high |
| Fit with Aurora-first | full | full | full | partial | partial | conflicts |

## Decisions

> **Status: confirmed — Scheme B selected.** `sky::ui` (`engine/ui`) is the editor UI framework, rendered by a
> new Aurora UI pass, driven by a thin native shell. The route matrix remains as the record of alternatives
> considered.

1. **Selected target: Scheme B.** Reuse `engine/ui` (`sky::ui`) as the editor's UI framework, extend it with
   the editor shell (docking, property grid, tree/table, text editing/IME), implement the `engine/ui/render`
   Aurora UI pass, and drive the editor from a thin native shell. It delivers a self-hosted, RHI-drawn editor
   while reusing the tested, render-agnostic core and enabling one UI stack for editor and runtime — the same
   shape as UE (Slate) and Godot (Control). A, A2, D, E, F are rejected above.
2. **Rendering & windowing: topology (1).** One main native window and one Aurora swapchain; the editor UI pass
   and the viewport are composited into the same frame. Extra swapchains only for explicitly detached windows.
   The legacy per-viewport child window (`Renderer::CreateRenderWindow`) is retired; the native handle flows
   through `SwapChain::Descriptor.window`.
3. **Native platform layer, not native widgets.** Win32 and Cocoa backends own window creation, event loop,
   input, IME, clipboard, and file dialogs. No editor panel is an OS widget (UE `GenericApplication` / Blender
   GHOST / Godot `DisplayServer` model). **Menus follow the UE/Blender/Godot precedent: the editor menu bar and
   toolbars are engine-drawn with `sky::ui`** (task 10.2), so they are consistent across Windows and macOS and can
   host editor commands directly. Native menu APIs are used only where the OS requires them — the macOS
   application/global menu — plus optional native context menus; this is scoped in task 7.4.
4. **Core services are toolkit-independent.** Undo/redo, the property model, documents, selection, and module
   registration are defined without UI-toolkit types and are headless-testable; panels are views over them
   (matching UE's reflection-driven `IDetailsView`). This lets the core land and be tested before the UI route is
   finalized.
5. **ImGui is runtime/debug-only.** It is excluded from the editor shell; it remains available for the runtime
   console, data inspectors, and gizmo overlays.
6. **Route-independent foundation first.** Native shell, Aurora renderer + UI pass, viewport, core services, and
   toolkit-agnostic module registration land before committing to a panel framework.
7. **Module layout: the rebuild lives in `engine/sandbox`; the legacy `engine/editor` is untouched.**
   - `engine/sandbox/core` → `EditorCore` (STATIC) is the home of the decoupled services, linking only
     `Core` + `Framework`, with a configure-time guard against `aurora/`, `render/`, `ui/`, and Qt includes. It
     builds with or without Qt, and also when only `SKY_BUILD_TEST` is on (so the tests run in normal test builds).
   - `engine/sandbox/src` → `Sandbox` (opt-in `SKY_BUILD_SANDBOX`) is the non-Qt shell prototype that drives
     `EditorCore`; `engine/sandbox/test` → `EditorCoreTest` holds the headless tests.
   - The legacy Qt editor (`engine/editor`) is deprecated and is **not modified**: the new editor is rebuilt in
     `engine/sandbox` from scratch rather than migrated in place.
8. **Phase 1 borrows the existing native window framework and stays RHI-free.** Validation reuses `Framework`'s
   `Platform` + `NativeWindow` rather than new platform code. `Sandbox` opens a non-Qt window and pumps events and
   deliberately does **not** link Aurora or the render stack. Aurora present is a separate later step and must not
   pull RHI into the sandbox shell target.
9. **Layout is a toolkit-independent model (`editor-layout`).** Docking is a data model (a split/tab/panel tree
   plus a panel registry) with JSON save/restore and reset-to-default; the shell renders it with draggable
   splitters and tab bars into the single main window. `sky::ui` supplies only anchored element placement, so it
   is not asked to host a docking container. Floating/detached panels are deferred.
10. **Editor is one process; play/PIE is a separate child process.** The editor never runs the game runtime in its
    own process (process-wide singletons, one RHI device, one logger callback would collide; crashes must not take
    down the editor). Play launches a `--pie` game process over a versioned IPC channel; PIE starts with a
    separate game window and may later share a GPU texture into the editor viewport. Cook/import use worker
    processes; modules/extensions stay in-process.
11. **The editor owns its own renderer.** `EditorRender` records its passes on the single Aurora device and
    presents to the editor swapchain, implementing the GUI pipeline and the UI pass independently of the engine
    scene pipeline. This lets editor UI bring-up proceed without the unfinished engine top-level renderer; the 3D
    viewport is composited later (offscreen scene target, or a shared texture from a PIE process). No second
    device is introduced.

## Risks / Trade-offs

- **[Aurora top-level renderer + UI pass unimplemented]** → `engine/aurora/core/Renderer.h` is a TODO and
  `engine/ui/render` is a placeholder. Mitigation: Phase 2 renderer + UI pass with an explicit spike and a
  visible single-quad milestone before panel work.
- **[Docking is hard and unspecified]** → scope a minimal docking model (tabs + splitters + named areas +
  save/restore layout); defer floating/detached panels and multi-window.
- **[Text editing + CJK IME is the highest-risk item]** → plan a native IME bridge early (GHOST-style); validate
  with a CJK input spike before porting EditBox-dependent panels.
- **[Property grid parity with `ReflectedObjectWidget`]** → decouple the descriptor model first, then re-host the
  view; treat parity (numeric/vector/color/asset/enum/array editors) as its own deliverable.
- **[Undo/redo semantics across mixed edits]** → define scope (per-document vs global), selection restoration, and
  dirty marking up front; route all property edits through the generic reflection command.
- **[Large content browser / tree performance]** → virtualization lives in the framework (retained list/table
  views), not in each panel; include a stress test.
- **[High-DPI and multi-window/DPI changes]** → `sky::ui` already has a DPI scale; verify per-monitor DPI and
  scope multi-window carefully.
- **[Long migration keeps two stacks alive]** → keep the Qt editor buildable through transition behind the
  existing `Qt5_FOUND` guard; delete only after parity for shipping workflows.
- **[Sandbox window harness needs a display]** → `Platform::PoolEvent` requires a windowing environment, so the
  visual harness is a manual/dev check, not a headless CI test; keep `EditorCore` tests headless instead.
- **[Module deploy deps are per-executable]** → when Aurora present is introduced later, shared modules
  self-register via `sky_add_dependency(... DEPENDENCIES Launcher Editor)`, so the RHI-linked render host must be
  added to those lists. The sandbox shell target is intentionally not one of them.
- **[Scope creep from "shell" into "editor features"]** → hold the line: parity of existing panels only, no new
  editor features in this change.

## Migration Plan

```
Phase 0  UI route decided: B (sky::ui + Aurora UI pass + native shell)
Phase 1  Route-independent foundation, NO RHI
         • Module scaffolding in engine/sandbox: core (EditorCore, configure-
           time guarded), src (Sandbox shell), test (EditorCoreTest);
           opt-in via SKY_BUILD_SANDBOX, tests via SKY_BUILD_TEST
         • Core services first (no UI): undo/redo stack, property model,
           documents, selection, toolkit-agnostic module registration
         • Native shell: BORROW Framework Platform + NativeWindow (SDL-backed
           Win32/Cocoa) instead of new backends; Sandbox opens a non-Qt window
           and pumps events; sandbox stays native-window-only with no Aurora or
           RHI dependency                                     [validated]
         Milestone: non-Qt native editor window + EditorCore validated
Phase 2  Panel framework + rendering
         • Docking/layout, menu/toolbar, theming, focus & input routing;
           inspector/outliner render the property model and selection
         • Aurora top-level renderer + engine/ui/render UI pass
           (spike → visible quad)
         • Introduce Aurora present as a SEPARATE RHI-linked host driving
           ClientViewport from the native handle, topology (1); do not pull RHI
           into the sandbox shell target
         Milestone: non-Qt editor window with one docked Aurora viewport
Phase 3  Port panels (parity only)
         • World outliner → Inspector/property grid → Console → Asset browser →
           asset editors (material/skeleton/animation) → gizmo overlay
Phase 4  Cut over & remove Qt
         • Delete Qt5 find/AUTOMOC/windeployqt from engine/editor; update module
           configs; retire the legacy render path from the editor
```

Rollback: the Qt editor remains buildable through Phases 1–3 behind the existing `Qt5_FOUND` guard; the new shell
lands as a separate target until Phase 4. Because the core services are toolkit-independent, Phase 1 carries no
UI-route risk.

*Optional accelerator*: if a shorter bridge is needed, insert **Scheme C** as a temporary stage (Qt6 shell + Aurora
viewport), then continue to Phase 2; the core services from Phase 1 remain valid.

## Open Questions

1. **UI route: DECIDED — Scheme B** (reuse `sky::ui`, rendered by Aurora, native shell only). A/A2/C were not
   selected.
2. **UI reuse: DECIDED — yes.** The editor UI and runtime UI share one stack (`sky::ui`), Godot-style.
3. **Docking scope**: the baseline (`editor-layout`) is tabs + splits + saved layout + reset-to-default; are
   floating/detached panels and multi-window required later (task 11.6)?
4. **Undo/redo scope**: per-document stacks or a single global stack with document tagging?
5. **IME strategy**: build the native IME bridge in Phase 1, or accept ASCII-only text editing initially?
6. **Qt end state**: remove Qt from the repository entirely, or keep it for other tools?
7. **Appetite/timeline**: is a multi-phase migration acceptable, or is a time-boxed Scheme C bridge required
   first?
