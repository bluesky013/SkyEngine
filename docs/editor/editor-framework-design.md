---
title: "Editor Framework Design"
description: "Startup/project flow and overall layout design for the sandbox editor (non-Qt)."
updated: "2026-10-05"
---

## Scope

Design for the **sandbox editor only** (`engine/sandbox`, `sky::ui`, non-Qt). Covers:

1. **Startup & project manager** — the editor binary (`SandboxEditor`) shows a project hub when no
   project is given, supports creating/opening a project, and then re-launches bound to that project.
2. **Overall editor layout** — the arrangement, panels, menus, toolbars, and workspaces, informed by
   Unreal, Godot, O3DE, Unity, and Blender.
3. **Asset / resource model** — engine / workspace / plugin assets vs application resources, storage
   scopes and the DDC, and cross-platform considerations (macOS / Linux).

It deliberately reuses the existing capabilities (`editor-layout`, `editor-ui-shell`,
`editor-global-config`, `editor-ui-icons`, `derived-data-cache`) and reserves hooks for an **AI agent**
(§10).

This document describes the **target** design. Where it cites current code (`file:line`) that is
**grounding, not a constraint** — the implementation may restructure existing pieces to meet the
design.

## Concepts

- **Application resources** (`<bundle>/resources/`): owned by the **application** (editor/runtime
  chrome — icons, fonts, skins, UI data), deployed beside the executable. They are **not assets**: not
  in the asset database, not part of any workspace/engine bundle, not browsable/cookable. Editor
  chrome icons (e.g. `resources/icons/save.svg`) are application resources and use the DDC **engine**
  scope.
- **Assets**: data managed by the asset system (UUID identity, builders, cook, mounts). They live in
  three places:
  - **Engine bundle assets** (`<bundle>/assets/`) — read-only, engine-owned (shaders, default
    materials, built-in content).
  - **Workspace assets** (`<project>/assets/`) — the project's writable source assets.
  - **Plugin assets** (`<plugin>/assets/…`, where `<plugin>` = `<bundle>/plugins/<id>/`) — read-only
    content shipped by optional plugins (see §9).
  The asset browser browses **assets** (engine bundle + workspace + plugin roots), never application
  resources.
- **Engine bundle** = the deployed engine/editor install beside the executable (`configs/`,
  `resources/`, `assets/`, `templates/`). **Workspace** = a project directory (`assets/`, `configs/`,
  `*.skyproj`).

## Non-goals

- Multi-viewport, remote/network editing, live coding / hot reload.
- A `native/` C++ project toolchain (CMake generation + build integration).
- A runtime asset pipeline beyond the mount + cook described in §2.5.
- Crash auto-save/backup and source-control integration.

## Implementation status

Current implementation state, build/run, gotchas and handoff live in **`editor-framework-status.md`**
(single source of truth). In short:

- **Implemented**: the `SandboxEditor` hub + editor shell; interactive docking/floating for
  `editor-interactive-docking` (Win32); per-user layout persistence.
- **v1 simplifications**: in-process project open (no `re-exec`); the hub is drawn in the main window.
- **Removed / not yet**: SDL is removed (macOS unsupported pending a native backend); asset browser,
  `--safe-mode`, named workspaces and PIE are not implemented.

## Entry & startup boundary (decided)

- **Two binaries, separated by build target** (UE-style):
  - **`SandboxEditor`** — the **only editor entry**. Hosts the **Project Manager (hub)** plus the
    editor: `SandboxEditor` (no `--project`) → hub; `SandboxEditor --project <path>` → editor bound
    to that project.
  - **`Launcher`** — the **game runtime only** (`Launcher --project <path>` runs the project; it is
    also what PIE spawns). No hub, no editor.
- **Remove the redundant editor path**: delete `Launcher --app editor` and the `SKY_EDITOR_HOST`
  gate (`Win32Launcher.cpp` currently also constructs `EditorApplication`).
- **Editor code never links into the game by construction** (target separation), not by a runtime
  gate; this satisfies the `AGENTS.md` rule that runtime must not depend on editor/adaptor.
- **Restart/re-exec**: the hub re-execs `SandboxEditor --project <path>` (Win32 `CreateProcess`;
  macOS/Linux `posix_spawn`) and exits; a present `--project` means no hub.
- **CLI forwarding**: the hub re-execs with the project path resolved to **absolute** and forwards the
  session flags (`--rhi`, `--safe-mode`).
- **Instances**: **one editor instance per project**, enforced by a **hard lock**
  (`<project>/cache/editor.lock`, holding the owner PID). Multiple **different** projects may run
  concurrently as separate processes, but a second editor on the **same** project is **refused** (no
  override) — the editor owns shared mutable project state (asset DB, project cache, thumbnails,
  cooked products). The hub offers *Focus existing* / *Cancel*; a stale lock (owner PID not running)
  is reclaimed automatically.
- **PIE = separate process**: play spawns `Launcher --project <path>` as an independent process
  (UE "Standalone"-style isolation), never rendering the game inside the editor process.
- **Hub = a separate Project Manager window on the base UI stack**: in hub mode `SandboxEditor`
  loads its **engine-side** base modules (`<bundle>/configs/modules_editor.json`: UI/render/text/
  resources) but **does not build the editor shell**; it opens the Project Manager in its **own
  top-level window** (`ProjectManagerApplication`). `--project` additionally mounts the project work
  FS, loads the project manifest modules, and builds the editor shell window. There is **no circular
  dependency**: the base stack is engine-provided, not from the project. Implementation: the host
  branches on project presence — `SandboxModule::Init` must **skip editor-shell construction in hub
  mode**.
- **Command-line contract**:
  - `SandboxEditor [--project <path>] [--rhi <name>] [--console] [--safe-mode]`
  - `Launcher [--project <path>] [--app game|xr] [--rhi <name>]`
- **File association**: opening a `.skyproj` maps to `SandboxEditor --project <path>`; launching with
  no argument opens the hub.

## 1. Current state (grounded)

- `Launcher` (`engine/launcher/windows/Win32Launcher.cpp`) is a thin mode dispatcher:
  `--app xr` → `XRApplication`, `--app editor` → sandbox `EditorApplication`, else `GameApplication`.
  There is **no project concept** at runtime.
- The editor host loads modules from `<bundle>/configs/modules_editor.json` (fallback `SandboxModule`)
  and mounts the **bundle path** as its work filesystem (`EditorApplication.cpp`).
- `SandboxModule::Init` builds the default dock layout via `LayoutModel` operations
  (`SandboxModule.cpp:77-82`): left column = `outliner`/`inspector`/`config` tabs, center
  `viewport`, right `refldemo`, bottom `outputlog`/`console` tabs; plus a `ToolBar` row of
  Show/Hide items.
- Layout model + registry + persistence already exist (`editor-layout`); persistence is currently
  test-only. `editor-global-config` has a seam (`IEditorConfigSource`) that is not yet wired.
- **Two editor entries exist today**: `SandboxEditor.exe` (`engine/sandbox/app`) and
  `Launcher --app editor` (`engine/launcher/windows/Win32Launcher.cpp`, gated by `SKY_EDITOR_HOST`),
  both constructing `editor::sandbox::EditorApplication`. The decision above collapses this to one.

## 2. Startup & project manager

### 2.1 Modes

| Entry | Behavior |
|---|---|
| `SandboxEditor` (no args) | **Project Manager** (hub). |
| `SandboxEditor --project <path>` | Editor bound to `<path>`. |
| `SandboxEditor --safe-mode` (flag) | Hub/editor with tool scripts / editor plugins / addons / scene restore disabled. |
| `Launcher --project <path>` | Game runtime for the project (also the PIE target). |
| `Launcher --app game \| xr` | Game / XR runtime (unchanged). |

The hub and the editor share the **`SandboxEditor`** binary (UE-style: the hub lives in the editor
binary, like Unreal's Project Browser). After a project is chosen the hub **re-execs**
`SandboxEditor --project <path>` (a clean module/config/asset state) and exits; a present `--project`
skips the hub. The game runtime is a **separate** binary (`Launcher`) and never contains the editor.

- **RHI is owned by the render side, not the editor**: both `SandboxModule` (editor,
  `SandboxModule.cpp:50`) and `AuroraModule` (runtime, `AuroraModule.cpp:55`) already parse `--rhi`
  (`API {DEFAULT, VULKAN, METAL, DX12}`) and pass the API to the renderer. The editor adds **no** RHI
  setting of its own; if `--rhi` is absent it forwards the project's `settings.rhi` (if any) as
  `--rhi` — precedence `--rhi` > project `settings.rhi` > default, done by forwarding.

### 2.2 Project format

A project is a directory:

```
MyProject/
  MyProject.skyproj      # identity + engine version + settings + module/plugin manifest
  configs/               # runtime config only (input map, autoloads, ...); settings live in .skyproj
    input.json
  assets/                # source assets (mounted as the WORKSPACE source root)
  native/                # (future) optional project C++ (game module/targets)
```

`*.skyproj` (descriptor) fields:

```jsonc
{
  "id": "<uuid>",              // stable identity (recent list, caches)
  "name": "MyProject",
  "engineVersion": "0.1.0",    // association; drives upgrade prompts
  "defaultScene": "scenes/main.sky.scene",
  "settings": { "rhi": "vulkan" },
  // Manifest entries (Gem/Package style): id + version + type + dependencies + enabled.
  "modules": [
    { "id": "MyGameModule", "version": "0.1.0", "type": "runtime", "enabled": true, "dependencies": ["RenderAdaptor"] },
    { "id": "MyGameEditorModule", "version": "0.1.0", "type": "editor", "enabled": true }
  ],
  "plugins": [
    { "id": "terrain", "version": "0.1.0", "enabled": true },
    { "id": "vegetation", "version": "0.1.0", "enabled": false }
  ]
}
```

`modules` = code modules loaded by the target; `plugins` = optional feature packages (enable/disable)
that can bring modules + assets + editor tools (plugin entries carry no target `type`).

- **Version compatibility**: manifest entries carry a `version`; resolution refuses a project whose
  `engineVersion` is newer than the running engine (min-version semantics); a full semver solver is a
  later refinement.

`settings` holds **project-level configuration** (e.g. `rhi`); the editor **forwards `rhi` as `--rhi`**
to the render side (parsed by `SandboxModule`/`AuroraModule`, which own RHI), and the Project Settings
menu edits this block — the editor does not add a separate RHI setting.

Asset references — including `defaultScene` — are **asset paths relative to the workspace asset root**
(`<project>/assets`), the same space the asset browser and the asset system use.

Rationale vs other engines (grounded):

- **Unreal** — a project is a directory with `<Name>.uproject` (JSON: `FileVersion`,
  `EngineAssociation`, `Modules[]`, `Plugins[]`). Launched with no project, `UnrealEditor` shows the
  **Project Browser** (Recent/Games/Engine lists, templates, and a target-platform + quality-preset
  step); opening a project loads the editor for it.
- **Godot** — a project is a directory with `project.godot` (name, `run/main_scene`, autoloads,
  input map, rendering, …) plus a `.godot/` cache. Launching Godot shows the **Project Manager**
  (Create / Import / Scan; per-project renderer selection; project tags; recent list; a file browser
  with Favorites/Recent). Opening (or creating) a project loads the editor for it. It also offers a
  **recovery mode** that disables tool scripts / editor plugins / addons / automatic scene restore
  when a project crashes on startup.
- **Unity** — the **Unity Hub** is a separate application that lists/opens/creates projects and picks
  the matching editor version; a project is a folder with `Assets/`, `ProjectSettings/`, `Packages/`
  and a `ProjectVersion.txt` pinning the version.
- **Blender** — has **no project hub**: preferences are user-global and work is `.blend` files (a
  "Template" system exists under File > New).

Common shape: a single human-readable descriptor keyed by a stable id with an engine-version
association, **a manifest of modules/plugins** (Gem/Package style), and a hub that is a
recent-list + templates + create/open surface.

### 2.3 Module/config resolution (layering)

- **Interface + data live in `engine/framework`** (consumer side, per `AGENTS.md`): a
  `ProjectDescriptor` type and an `IProject`/`ProjectService` seam returning the descriptor, the
  work filesystem, and the **resolved, ordered module/plugin set**. Nothing in `engine/*` depends on
  a plugin.
- **Base vs project modules**: the editor binary always loads its **engine-side** base list
  (`<bundle>/configs/modules_editor.json`); the project manifest only **adds** project modules. The
  resolver topologically orders by `dependencies` and drops `enabled:false`.
- **Target filter**: the resolver is target-aware — the **editor** loads `editor`+`runtime` modules;
  the **game** (`Launcher --project`) loads `runtime` only. This is what keeps editor modules out of
  the game.
- This replaces the ~4 duplicated module-list JSON parsers (plus the legacy Qt editor) with one
  framework helper.
- **Work filesystems**: the runtime work FS is the **project dir** (`AssetManager::SetWorkFileSystem`);
  the authoring `AssetDataBase` mounts `workSpaceFs` = `<project>/assets` and `engineFs` =
  `<bundle>/assets`, composed workspace-first (`MultiFileSystem`). The **sandbox host does not wire
  these yet** (only the legacy editor does) — P0 wires them.
- **Recent projects**: persisted under `Platform::GetUserConfigPath()` (e.g. `projects.json`);
  mirrors the Python `project_manager.ini` intent but at runtime.
- **Settings layering** (low → high precedence): **engine defaults** (built-in engine `configs/` +
  code defaults) → project `<project>/.skyproj settings` → user preferences (`GetUserConfigPath()`) →
  command line. RHI is **owned by the render
  side** (`--rhi`, parsed in `SandboxModule`/`AuroraModule`); the host only forwards the project's
  `settings.rhi` when the flag is absent.
  `editor-global-config` (`IEditorConfigSource`) is the seam for the user layer; the project layer is
  the descriptor's `settings` block.

### 2.4 Project Manager (hub)

- Layout: left = recent projects list (name, path, engine version, last opened), right = engine
  version + template gallery + New/Open actions.
- Actions: **New**, **Open**, **Open recent**, **Browse**, **Remove from list**; each project row
  shows a version-mismatch badge (like Unreal).
- **New project**: name + parent directory + template (Empty / Sample 3D / from template dir). The
  hub creates the directory layout above, writes the `.skyproj`, seeds `configs/` from engine
  defaults, and then re-execs the editor bound to the project. (Godot/Unreal both offer templates here.)
- **Templates**: shipped under `<bundle>/templates/<name>/` with a `template.json`
  (`name`, `description`, `thumbnail`, descriptor seed) plus the files to copy. Instantiating a
  template copies its files into the new project dir, writes the `.skyproj` from the seed, and mints a
  fresh `id`.
- **Version mismatch / upgrade**: when the project's `engineVersion` differs from the running engine,
  the hub flags it (badge) and offers **Open anyway** (warn) or **Upgrade** — a versioned migration
  path (descriptor/asset schema steps) that then rewrites `engineVersion`. Opening a project newer
  than the engine is refused.
- **Hub window**: `ProjectManagerApplication` is a **separate top-level window** (its own layout —
  recent list + template gallery + actions) built on the same base UI/render stack as the editor but
  **without the editor shell**. Selecting a project re-execs `SandboxEditor --project`, which builds
  the editor shell window. No OS widgets. The hub window is the process **main window**; closing it
  exits the app.
- **Single instance per project**: the editor acquires a **hard lock**
  (`<project>/cache/editor.lock`, owner PID) on open; an already-locked project is **refused**
  (*Focus existing* / *Cancel*, no override). A stale lock (owner gone) is reclaimed automatically.
  Different projects run concurrently. See "Instances" in *Entry & startup boundary*.

### 2.5 Engine / workspace / plugin asset resolution (mount, not copy)

The engine already implements a **flat overlay** for the two shared namespaces: `AssetDataBase`
composes `engineFs` (read-only `<bundle>/assets`) with `workSpaceFs` (the writable project source root
`<project>/assets`) into one `MultiFileSystem`, where the workspace **shadows** the engine for the same
path (`RebuildMounts` / `ResolveOwningFs`). **Plugin roots are not part of this flat overlay** (see the
Plugins bullet and §9). Note: the `pluginFs` member exists in the code but is a **stub** — it is never
populated, so plugin mounting is currently unimplemented. This design **keeps mount semantics**:

- **Editor**: engine assets are mounted read-only; project assets are the writable workspace; a
  project path shadows the engine path of the same name. Projects **never** copy engine assets into
  the project directory.
- **Plugins**: each plugin's content is a **separate, read-only, namespaced root** (`/plugin/<id>/…`)
  resolved by a **prefix-aware lookup** — **not** merged into the engine/workspace `MultiFileSystem`.
  This keeps plugin content collision-free and clearly owned (UE `/<Plugin>/`, O3DE `@gemroot:<gem>@`).
  Optional project overrides of plugin assets are **explicit** by prefix, never implicit shadowing.
- **Cook/package**: only assets that are actually referenced are cooked into the project's product
  bundles (a selective copy at build time) — this is where "copy" happens, mirroring Unreal's cook.
- **Runtime (game)**: `Launcher --project` loads the project's **cooked product bundles**, not source
  assets; product bundles mount with the same rules — engine bundles read-only, project bundle
  shadowing, and **plugin content resolved by the same prefix-aware lookup** (namespaces preserved at
  cook) — via `AssetManager`/bundle FS.
- **New project from template**: only the template's own files are copied into `assets/`.

How other engines do it: Unreal mounts engine content read-only (`/Engine/` vs project `/Game/`,
plugins as `/<Plugin>/`) and **cooks only referenced assets** into a packaged build (`.uproject`
`EngineAssociation` locates the engine); Unity keeps project assets in `Assets/` and resolves packages
into a **copy** in `Library/PackageCache` (editor built-in resources live in the editor install);
Godot **embeds** engine built-ins in the binary and **copies** templates into the project; Blender has
no engine asset tree and offers explicit **Link** (reference) vs **Append** (copy).

**Decision: mount/reference engine assets; copy only at template-create and at cook.**

Broader survey (how engines relate engine vs project resources):

- **Mount / overlay + build-time cook (dominant):** Unreal (`/Engine` vs `/Game`, cook → pak);
  Valve Source (`gameinfo.txt` search paths + VPK, first-match-wins, mods reference engine `platform/`
  `hl2/`); id Tech (`base/` + mod dirs + `.pk3` archives); CryEngine/Lumberyard (`Engine/` vs
  `Game/`, pak); O3DE (Gems contribute asset roots mounted with the project; Asset Processor copies
  source → `Cache/`); Flax (`Content/` mounts, cook on build); Bevy (`AssetServer` pluggable
  `AssetSource`s, `AssetPath` with a `source://` prefix); Panda3D (multifile/`.p3d` mounts).
- **Resolve then copy into a project cache:** Unity (`Packages/` → `Library/PackageCache`; imports →
  `Library/`); Stride (packages + `Assets/`, processed to the output).
- **Engine built-ins embedded + copy templates:** Godot (built-ins in the binary; `res://` is the
  project root; templates/addons copied in); Defold.
- **Content compile:** MonoGame/XNA (`Content/` → `.xnb` copied into the build).
- **No engine asset tree:** Blender (explicit Link vs Append).

Takeaway: engines with a distinct engine-asset library almost all **mount + cook/pak**; copying into
the project only happens for package caches (Unity/O3DE) or templates (Godot). Our flat
`engineFs` + `workSpaceFs` overlay (workspace shadows engine) **plus namespaced plugin roots** + cook
of referenced assets is squarely in the dominant category.

## 3. Overall editor layout

### 3.1 Reference comparison (grounded)

| Aspect | Unreal | Godot | Blender | Unity |
|---|---|---|---|---|
| Startup surface | Project Browser | Project Manager | none (`.blend`) | Unity Hub (separate) |
| Workspace tabs | Editor Modes | 2D / 3D / Script / AssetLib | Workspaces | Layouts |
| Non-overlap | Overlapping docks | Dockable | Strict, tiled | Dockable |
| Outliner | World Outliner | Scene tree | Outliner | Hierarchy |
| Inspector | Details | Inspector | Properties (left tabs) | Inspector |
| Assets | Content Browser / Drawer | FileSystem dock | Asset / File Browser | Project window |
| Console/log | Output / Message Log | Output bottom panel | Console / Info | Console |
| Play | PIE (option: separate process) | Play | (viewport render) | Play mode |

Grounded notes:

- **Godot** — main screens `2D / 3D / Script / AssetLib`; docks (Scene, FileSystem, Inspector, and a
  bottom tab of Output/Debugger/Audio/Animation); dockable with named **layout presets** ("Editor
  Layout").
- **Blender** — a window is a **Topbar** (menus + **workspace tabs** + scene/view-layer selectors) plus
  a **Status Bar**, and is tiled into non-overlapping **Areas**; each Area has an editor type and its
  own **Header** (plus optional toolbar/sidebar **Regions**); the Properties editor groups into left
  tabs.
- **Unreal** — docking tab-stacks; **Editor Modes** (Select/Landscape/Foliage/Modeling/…); Content
  Drawer; **Play-in-Editor** (with a separate-process option); per-user saved layouts + "Reset Layout".

Takeaways: a **dockable**, non-overlapping base (UE/Godot/Unity) plus **named workspaces** and a
**status bar** (Blender/Godot) and an explicit **mode enum** is the right combination; a per-area
**header** (Blender) generalizes our existing tab bars.

### 3.2 Proposed default layout

```
+---------------------------------------------------------------------+
| MenuBar: File  Edit  View  Window  Tools  Help                      |
| Workspaces: [Layout]  Modeling  Debug  Script                       |
| ToolBar: [save]  [select][move][rotate][scale]  [play*]             |
+----------------+--------------------------------------+-------------+
|  Outliner      |              Viewport                |   Details   |
|  (hierarchy)   |       (scene view, gizmo)            | (inspector) |
|                |                                      |             |
+----------------+--------------------------------------+-------------+
|  Content Browser | Output Log | Console                             |
+---------------------------------------------------------------------+
| StatusBar: project | engine ver | RHI | fps | mode | selection       |
+---------------------------------------------------------------------+
```

- Ratios (default): left `0.18`, right `0.24`, bottom `0.28`, top chrome (`menu` + `workspace strip` +
  `toolbar`) `~72px`, status `~22px`.
- `play*` renders **disabled** until the scene-render path exists (see §3.6).
- The **Workspaces** strip is a top chrome row (Blender/Godot style) that switches named layouts (§3.3).
- The area model already supports split/tab/drag (`editor-layout`); this design only fixes the
  **default arrangement** and adds the **menu bar**, **workspace strip**, **status bar**,
  **content browser**, and **workspaces**.

### 3.3 Workspaces (Blender/Godot influence)

Named, saved layouts on top of `LayoutModel`:

- **Layout** (default, above), **Modeling**, **Debug** (adds console/log/stats), **Script/UI**.
- Switching a workspace loads a named layout; the current layout auto-saves per workspace.
- Uses `editor-layout` persistence (`LayoutPersistence`), extended to store a workspace map
  **per user** (under `GetUserConfigPath()`), not inside the project.
- Capability: **`editor-workspaces`**.

### 3.4 Panels

Existing: `viewport`, `outliner`, `inspector`, `config`, `outputlog`, `console`, `refldemo`.
Proposed additions: `content` (asset/file browser), `history` (undo/redo + operations), `stats`
(debug), `scene` (scene/settings), `ai` (AI panel, reserved — §10), and *placeholder* removal of
`refldemo` from the default set (keep registered, off by default).

Capability mapping: chrome + menu/toolbar/status → **`editor-actions`**; default dock arrangement →
`editor-layout` (existing); named workspaces + per-user persistence → **`editor-workspaces`**;
asset/file browser → **`editor-content-browser`**; AI assistance (reserved) → **`editor-ai`** (§10);
floating panel tear-out (reserved) → **`editor-floating-docking`** (§3.9).
The existing `config` panel overlaps the Project Settings…/Preferences… menus — fold it into those
(keep only a lightweight quick-settings surface, or retire it).

### 3.5 Menu structure

`File` (New/Open Project, Save, Save All, Recent, **Project Settings…**, Exit) ·
`Edit` (Undo/Redo, **Preferences…**) · `View` (panel visibility, workspaces, fullscreen) ·
`Window` (reset layout, save layout) · `Tools` (RHI switch, asset cook) · `Help` (About, engine version).

- **Project Settings…** (File) edits the project-level config — the `.skyproj` `settings` block
  (including `rhi`); **Preferences…** (Edit) edits **user-level** editor prefs
  (`GetUserConfigPath()`). These are the menu surface for the settings layering in §2.3.
- The Tools "RHI switch" reflects the **resolved** RHI (owned by the render module; the switch writes
  the project `settings.rhi` and re-applies via re-exec).

Menus, toolbars, and hotkeys are driven by a **declarative action registry** (O3DE Action-Manager
style: an `Action` bound into `Menu`/`Toolbar` with sort keys + visibility); the current hard-coded
flat toolbar items are replaced by this registry — new capability **`editor-actions`**.

### 3.6 Viewport

- Selection + gizmo (translate/rotate/scale) tools; grid/snapping toggles.
- **Editor modes**: `Edit` (authoring), `Simulate` (game logic runs inside the editor viewport), and
  `Play` (separate process). v1 is `Edit` only; `Play` lands with the scene-render path; `Simulate` is
  later.
- **Preview window**: by default the preview surface is a **dockable panel inside the editor main
  window** (a normal tab/area). Tearing it out into an independent top-level window is a **reserved,
  post-v1** capability (§3.9); multi-window support is a cross-platform concern (§8).
- **Play-in-editor = separate process** (decided): play spawns `Launcher --project <path>` as an
  independent process (UE "Standalone"-style isolation), so the game runs with its own window and no
  editor state. The editor toolbar's play controls are shown but **disabled until the scene-render
  path exists** (the current `EditorRenderer` only clears).

### 3.7 Status bar

Project name, engine version, RHI/backend, fps/frame ms, current tool/mode, selection count. Cheap
to add and greatly aids orientation; reads from existing services.

### 3.8 Asset Browser (`editor-content-browser`)

The asset browser is the primary asset surface (UE Content Browser / Godot FileSystem dock / Unity
Project window):

- **Layout**: left = **folder tree** over the mounted **asset** roots (workspace `assets/` first, then
  engine bundle `assets/`, then plugin asset roots); center = **content area**; top = breadcrumb +
  search + type filter; a collapsible **details/preview** strip for the selection. Application
  `resources/` are **not assets** and are not shown here.
- **Views**: grid (thumbnail tiles) and list (columns: name / type / size / modified); sort + zoom.
- **Thumbnails**: previews are produced by **builders through the DDC** (a `thumbnail` builder beside
  `ui-icon-svg`), cached and requested lazily by the grid.
- **Navigation**: breadcrumb + back/forward history; **favorites** and **recent**; a "show engine
  content" toggle (engine assets hidden by default, UE-style).
- **Operations**: import (drag from OS / Import action), create (folder / material / scene / …),
  rename, **move by drag-and-drop**, duplicate, delete (to trash), reveal in the OS file manager.
  Asset **identity is stored in asset metadata and survives move/rename** (path is not identity).
- **Selection & open**: multi-select; single selection drives the inspector/details; double-click or
  drag opens the asset in a **document tab** (`editor-document`); drag into the viewport
  assigns/spawns.
- **Filtering**: by name + asset type; saved filters/collections (UE-style) later.
- The browser is a pure `sky::ui` panel; OS **file dialogs** are used only for import/reveal — the
  one place native UI is acceptable. The browser shows **source assets only** (the workspace + engine
  + plugin source overlay); cooked products are a runtime concern.

Notes / layering:

- The **thumbnail builder** must live on the **editor/adaptor** side (it needs asset loading +
  rendering), not a core builder — respecting the `AGENTS.md` builder/cook rules.
- **Import** goes through the asset builder; the browser is the UI over it.
- **Asset preview** (a 3D/texture preview pane or editor) is a later addition; v1 shows metadata in
  the details strip.
- **Drag into the viewport** (assign/spawn) depends on the scene + document/viewport capabilities
  and lands with them (later).

### 3.9 Window & docking model (floating tear-out — reserved)

**Current model (grounded).**

- Panels are **in-window UI**: the layout is a tree of `SPLIT`/`TAB` nodes with `PanelNode{ panelId }`
  leaves (`editor/core/layout/LayoutNode.h`), rendered by `EditorShell` into **one**
  `UIContext`/`UIPaintContext` — i.e. **one HWND** (the main editor window). Panels have **no HWND of
  their own**.
- Separate **top-level windows** are `NativeWindow`s (`Win32Window` → its own `HWND`, tracked in
  `NativeWindowManager`): the main editor window and the standalone preview window are two HWNDs.
  There is **no cross-window docking / reparenting**.

**Other engines.** Unreal (Slate `SWindow`), Unity, and O3DE (Qt `QDockWidget`) support
**docked-by-default + tear-out**, where each **floating panel is its own native window**; Godot does
**not** (its docks are fixed slots); Blender uses **multiple native windows tiled into Areas**
(duplicate-area-to-new-window), not automatic tear-out.

**Target (reserved, post-v1).**

- Add a `FLOATING` state to the layout model: a floating panel is bound to a `NativeWindow` (its own
  HWND) with **its own UI context + `ClientViewport`/swapchain**. **Tear-out** creates that window and
  moves the panel's rendering there; **re-dock** destroys it and restores the panel into the main
  tree. Floating state persists **per user, per workspace**.
- This is the same multi-window capability the **hub window** needs (§8).

**Not v1.** v1 keeps panels docked in the single main window (preview included). Hooks to keep now:
(1) the layout model stays extensible (add a node kind/state rather than assuming one window);
(2) the shell renders through a **per-window path** so a second window can reuse it; (3) the §8
multi-window items (close / DPI / focus) already cover floating windows.

## 4. Review rounds

**Round 1 — Unreal lens.** World Outliner / Details / Content Browser triad, Play-in-Editor
(separate-process option), Editor Modes, `.uproject` engine association + version-mismatch prompt,
recent projects with mismatch badges, per-user saved layouts + "Reset Layout".
*Adopted:* Content Browser, PIE, version association, mode enum, per-user layout storage.
*Deferred:* overlapping dock stacks with advanced restore (our model is non-overlapping), live
coding / hot reload.

**Round 2 — Godot lens.** Godot's same-binary **Project Manager** (Create/Import/Scan, favorites/recent,
tags), per-project renderer choice, template gallery, `2D/3D/Script` main screens, recovery mode.
*Adopted:* editor-binary hub, templates, named layout presets, recent list with tags, **recovery/
safe mode** (`--safe-mode`), a lightweight menu plus a compact menu bar.

**Round 3 — Blender lens.** **Topbar** (menus + workspace tabs), **Status Bar**, strictly
non-overlapping **Areas** each with its own **Header**, and Properties grouped into left tabs.
*Adopted:* named workspaces, per-area header (generalizes our tab bars), grouped properties
(inspector sections), a status bar. *Deferred:* Blender's area-swap operator model.

**Round 4 — Unity lens.** Separate **Hub** app, `ProjectVersion.txt` version pinning, `Assets/` +
`ProjectSettings/` + `Packages/` layout, dockable layouts selected from the toolbar.
*Adopted:* explicit version pinning in the descriptor, a `Package`-like plugin/module manifest.
*Deferred:* a separate hub executable — we keep **two binaries** (editor = hub + editor, game =
runtime).

**Consolidated decisions (post-review):**
1. **Editor binary owns the hub** (UE-style): `SandboxEditor` = hub + editor; `Launcher` = game
   runtime; the hub re-execs `SandboxEditor --project` (clean module/asset state). Editor code never
   links into the game.
2. Single JSON `.skyproj` + stable id + engine version (UE-like association).
3. Dockable, non-overlapping layout (existing `editor-layout`) as the base; **workspaces** layered
   on top (Blender/Godot).
4. Menu bar + toolbar + **status bar**; content browser; **PIE runs as a separate process**
   (`Launcher --project`).
5. Project abstraction interface in `engine/framework`; hub/editor implementation in `engine/sandbox`;
   the game runtime (`Launcher`) loads **runtime-only** modules.

## 5. Decisions & remaining questions

**Resolved**
- **Engine / workspace / plugin assets** — mount/reference; copy only at template-create and at cook (§2.5).
- **Entry & binaries** — the editor binary owns the hub (`SandboxEditor`); `Launcher` = game runtime;
  remove `Launcher --app editor` and `SKY_EDITOR_HOST` (see "Entry & startup boundary").
- **Restart** — the hub re-execs `SandboxEditor --project`; no in-process project switch.
- **PIE** — a separate process (`Launcher --project`).
- **Recovery/safe mode** — adopted: the hub passes `--safe-mode` (skips tool scripts / editor plugins
  / addons / scene restore).
- **Module/plugin manifest** — adopted: Gem/Package-style manifest entries (`id`/`version`/`type`/
  `dependencies`/`enabled`), resolved (topo-ordered) by an `engine/framework` helper.
- **Workspace storage** — per-user: named layout presets (Layout / Modeling / Debug / Script) and
  their panel arrangements are stored under `GetUserConfigPath()`, not inside the project.

**Remaining**
- **Native code projects (`native/`)** — *recommended: no* for v1 (editor + manifest module set
  only); a C++ toolchain (CMake generation + build integration) is deferred.
- **Script/UI editor** — include a Script/UI workspace tab now, or defer to its own capability?

## 6. Verification

- **Hub boot**: `SandboxEditor` (no `--project`) starts, shows the Project Manager, and closes cleanly.
- **Open / re-exec**: choosing a project re-execs `SandboxEditor --project`; the editor mounts the
  project work FS, loads the manifest modules, and opens `defaultScene`.
- **Game runtime**: `Launcher --project` loads **runtime-only** modules (no `editor` modules) —
  asserted by a manifest unit test.
- **Manifest resolver**: framework unit tests for topological order, `enabled:false` drop, target
  filter, and version association/mismatch.
- **Safe mode**: `--safe-mode` boots with tool scripts / editor plugins / addons / scene restore skipped.
- **Layout / workspaces**: per-user persistence round-trips (extends `LayoutPersistenceTest`).
- **Asset browser**: browsing workspace/engine/plugin roots, DDC-thumbnail generation, and
  move/duplicate keeping identity.
- **Plugin namespacing**: `/plugin/<id>/…` resolves in the plugin root (assets) and resource resolver
  (resources); nothing leaks into the flat engine/workspace mount.
- **AI agent (reserved, §10)**: the action registry and agent seam expose machine-readable actions.

## 7. Phasing

- **P0 `editor-project-manager`** — collapse to one editor entry (remove `Launcher --app editor` and
  `SKY_EDITOR_HOST`); hub mode = **separate Project Manager top-level window** on the base UI stack
  (editor shell built only in `--project` mode); framework `ProjectDescriptor`/`IProject` + manifest
  resolver (topo / enabled / target) and the `AssetDataBase` engine/workSpace mounts wired in the
  sandbox host (**workspace shadows engine**; plugin roots are namespaced, `pluginFs` is a stub to
  implement); `.skyproj` read/write; forward project `settings.rhi` as `--rhi` (parsed by
  `SandboxModule`); hub UI (new / open / recent / version pin); re-exec
  (`--project`) and `--safe-mode`; templates (seed configs/assets/manifest).
  *Cross-platform: Windows first; wire §8 (close event, multi-window, re-exec, file association).*
- **P1 `editor-actions` + `editor-layout-shell` + `editor-content-browser`** — declarative action/
  menu/toolbar/status registry; **Project Settings… / Preferences…** menu surface for the settings
  layering (§2.3/§3.5); default layout re-arrange (menu bar, status bar); **asset browser** (§3.8,
  including DDC thumbnails); `refldemo` off by default. *Cross-platform: §8 window/menu parity on
  macOS/Linux.*
- **P2 `editor-workspaces`** — named workspaces (Layout / Modeling / Debug / Script) with per-user
  `LayoutPersistence`.
- **P3 (later)** — PIE (separate process; needs the scene-render path), `native/` C++ projects,
  Script/UI editor, the **AI agent** (`editor-ai`, §10), and **floating panel tear-out**
  (`editor-floating-docking`, §3.9). Keep the action-registry / provider / per-window render seams
  data-described now.

## 8. Cross-platform considerations (macOS / Linux)

Windows-first, but macOS and Linux are targets. Parity is required at every layer:

- **Window close event + quit**: broadcast `IWindowEvent::OnWindowClose` and map main-window close to
  quit on **every** backend (Win32, macOS `NSWindow`, SDL on Linux), not just Win32.
- **Multiple top-level windows**: the hub window and the editor window require independent top-level
  windows per platform (multi-window `NativeWindowManager` on Win32 / `NSWindow` / SDL).
- **Re-exec / restart**: Windows `CreateProcess`; macOS/Linux `posix_spawn`. On macOS the editor ships
  as a **`.app` bundle** — re-exec targets the bundle executable and respects single-instance
  semantics; on Linux a binary + `.desktop`.
- **File association** (open `.skyproj`): Windows registry; macOS `Info.plist`
  `CFBundleDocumentTypes` (UTI); Linux `.desktop` `MimeType`.
- **Bundle / install layout**: `GetBundlePath()` resolves `Contents/MacOS` on macOS and an install
  prefix on Linux; `configs/`, `resources/`, `templates/` must resolve within that layout.
- **RHI per platform**: Windows Vulkan/DX12; macOS **Metal** (`API::METAL`); Linux Vulkan — selection
  stays with the render side (`--rhi`).
- **Metal toolchain**: macOS additionally needs the MSL shader path (slang/dxcompiler → SPIRV-Cross →
  MSL) — a distinct cross-platform dependency, not just an `API::METAL` switch.
- **Distribution form**: Windows exe; macOS `.app`; Linux binary + `.desktop`.
- **Module suffix / loading**: `.dll` / `.dylib` / `.so` — the manifest resolver is suffix-agnostic.
- **Paths**: `.skyproj` and manifest paths use `/`; Linux is case-sensitive; no backslashes; the
  `--project` path is resolved to absolute before re-exec.
- **PIE**: the spawned `Launcher` is the bundle executable / a `.app` on macOS; spawn is
  platform-specific.
- **Storage**: recent projects and per-user workspaces use `GetUserConfigPath()` (macOS
  `~/Library/Application Support`, Linux `~/.config`).
- **DPI / scaling**: the theme-metric/font DPI scaling is per platform (Win32 DPI awareness; macOS
  backing scale; Linux/X11/Wayland fractional scaling) — each window backend sets it.
- **Native file dialogs**: import/reveal dialogs are the one native-UI dependency and are wired per
  platform.

## 9. Storage & caches

| Scope | Location | Contents |
|---|---|---|
| **Application resources** | `<bundle>/resources/` | app chrome (icons, fonts, skins, UI data) — **not assets**; chrome icons are **pre-baked** (engine DDC scope) |
| **Engine bundle** | `<bundle>/assets/`, `<bundle>/configs/`, `<bundle>/templates/` | read-only engine **assets** + engine configs/templates |
| **Plugin** | `<bundle>/plugins/<id>/` (module + `assets/`, `resources/`, pre-baked derived) | read-only plugin **assets** + plugin UI **resources** (app-side, not assets) — **plugin DDC scope** |
| **Workspace (project)** | `<project>/assets/`, `<project>/configs/`, `<project>/*.skyproj` (+ a project derived cache) | writable project **assets** + settings + project-derived data (cooked products, project thumbnails) — **project DDC scope** |
| **User** | `GetUserConfigPath()` | recent projects, per-user workspaces/layouts, preferences, user cache (thumbnails) — **user DDC scope** |

- The DDC is **scope-aware**, with scopes `{ engine, plugin, project, user }`. Two storage rules:
  - **Built-in derived data (engine, plugin) is pre-baked at build time** and shipped inside the
    bundle (read-only). The install (`<bundle>`, incl. a macOS `.app` or a system install) is treated
    as **read-only** — runtime code must never write into it.
  - **Runtime caches go to writable locations only**: project-derived → `<project>/cache`; user-scoped
    (thumbnails / recent / workspaces) → `GetUserConfigPath()`. If an artifact isn't pre-baked, its
    runtime cache falls back to the **user** scope.
  Chrome **icons are engine-scoped** (application resources, not assets — pre-baked); asset-browser
  **thumbnails** and other asset-derived data are **project- or user-scoped** and are never written
  into the install.

### Plugin content

A plugin may ship **both** kinds. Note there are **two distinct manifests**: the project's `plugins[]`
entries (in `.skyproj`) **enable a plugin by id**; the plugin's own `plugin.json` declares `id`,
`version`, `contentRoot` (assets), and `resourceRoot` (resources). A plugin lives at
`<bundle>/plugins/<id>/` (that directory is what `<plugin>` means — see Concepts). The host registers
both roots on load. Then:

- **Plugin assets** (`<plugin>/assets/…`): registered by the plugin at load as a **separate, read-only,
  namespaced root** (`/plugin/<id>/…`) — **not** merged into the engine/workspace `MultiFileSystem`.
  Resolution is **prefix-aware**: a path starting `/plugin/<id>/…` resolves in that plugin's root; all
  other paths resolve through the flat engine/workspace overlay (workspace shadows engine). Referenced
  plugin assets are cooked into the product, **preserving the namespace** so editor and runtime
  resolution match. The asset browser shows each plugin root as a toggle-able node.
  *(Implementation gap: `AssetDataBase::pluginFs` is currently an unused stub — namespaced plugin
  mounting must be implemented.)*
- **Plugin resources** (`<bundle>/plugins/<id>/resources/…`): plugin UI chrome (panel/tool icons,
  skins) used by the **editor**. They are **not assets**: they are resolved by the **application
  resource resolver** (the one the editor's own chrome uses), extended with a **namespaced plugin
  root** (`/plugin/<id>/…`) — prefix-aware, and **not** merged into the app's flat `resources/` (the
  same namespacing principle as plugin assets). The root is registered on plugin **load** / removed on
  **unload** through an **engine-side resource-registry interface** (the resolver seam the engine
  defines; the plugin registers). Derived plugin chrome icons use the **plugin** DDC scope. Plugin
  resources are **not cooked** and are **not shown** in the asset browser.
- **Lifecycle**: registration/unregistration happens on plugin **load/unload**, through **engine-side
  interfaces** (the engine defines the registration seam, the plugin implements it — no engine→plugin
  dependency, per `AGENTS.md`).

## 10. AI agent (reserved)

AI assistance is a **reserved** capability, not v1 scope, but the design leaves seams so it can slot in
without rework:

- **Actions are the tool surface.** The declarative action registry (§3.5) is the agent's tool
  interface: actions are **registered, discoverable, id'd, and parameter-typed**, so an agent can be
  handed a machine-readable catalogue (id / description / parameters) and invoke actions to operate the
  editor (create/modify assets, run commands, change view). Design rule: keep actions
  **data-described**, not only C++ handlers, so they can be exposed as tools.
- **Engine-side capability seam.** An `IAiProvider`/`IAgentTool` interface lives on the consumer side
  (`engine/framework` or `engine/sandbox` core), so AI backends (local model, remote API, MCP server)
  are **plugins** — no engine→plugin dependency (per `AGENTS.md`).
- **UI**: a dockable **AI panel** (`editor-ai`) with a context provider (current selection / scene /
  asset / docs) and an action transcript; ships as a panel + plugin, off by default.
- **Safety**: agent-initiated edits go through the **same undo stack** as manual edits and respect
  permissions; destructive actions require confirmation (agent edits are ordinary editor actions).
- **Phasing**: P3+ / later. Hooks to keep now: (1) actions stay data-described; (2) define the agent
  provider seam as an engine interface; (3) reserve the panel id `ai`.
