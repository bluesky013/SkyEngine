## Context

The sandbox editor is fully engine-drawn (`sky::ui` + `UiSkin`) and does not use Qt. The Project Manager
hub already exposes `onAdd`/`onNew` hooks, but `New Project` hard-codes the user projects directory and
`Add Project` calls Win32 `GetOpenFileNameW` directly inside `SandboxModule`. The framework exposes only
`ShowOpenFileDialog`/`ShowSaveFileDialog` (native, file-oriented, no directory mode). Other engines solve
this with a platform file-dialog service (Unreal `IDesktopPlatform`, Unity `EditorUtility.OpenFolderPanel`)
and an engine content browser; here we want a single reusable, **engine-drawn** chooser so it looks and
behaves consistently on every backend and can be reused by New/Add/Open and later the content browser.

## Goals / Non-Goals

**Goals:** a reusable modal `FileBrowserDialog` with three modes (OpenFile / OpenProject / SelectDirectory),
extension filters, a location bar, a directory listing with navigation, and a name field; wire Project
Manager New (directory) and Add/Open (project); remove the ad-hoc Win32 call.

**Non-Goals:** asset thumbnails / preview, multi-select, search, virtual file systems, a full content
browser (`editor-content-browser`), and per-project mounts. Those build on this later.

## Decisions

1. **Headless model in `core`, widget in `shell`.** `FileBrowserModel` (current directory, entry listing,
   navigation, filter matching, selection, name) lives in `editor/core/filebrowser/` with no UI/toolkit
   types and is unit-tested. `FileBrowserDialog` (a `sky::ui::UIElement`) lives in `shell` and renders the
   model. This mirrors `LayoutModel`/`EditorShell` and keeps `core` dependency-safe.
2. **Engine-drawn modal, not native.** Consistent with the engine-drawn shell, portable, and reusable for
   the future content browser. A native directory dialog is left as an optional fallback seam on the
   platform abstraction, not the primary path.
3. **Modes and filters.** `FileBrowserMode { OpenFile, OpenProject, SelectDirectory }`. OpenProject is
   OpenFile with the `*.skyproj` filter preset. Filters are `{label, extensions[]}`; SelectDirectory hides
   the file filter and lists directories only, letting the user pick the current directory.
4. **Hosting and result flow.** The dialog is a reusable element with a `SetOnResult` callback. For the
   Project Manager hub (the reported flow) the dialog is hosted by `SandboxModule` in `hubContext` because
   the hub is not the `EditorShell`; the module builds the request (mode, title, initial directory, filters,
   default name), routes input to the dialog while it is open, and consumes the result. An
   `EditorShell::OpenFileBrowser` API for editor-mode file open is a deferred follow-up (task 3.1).
5. **Project Manager wiring.** `New Project...` -> SelectDirectory with a name field; on accept the module
   validates the target is empty/non-existing and calls `ProjectDescriptor::Create`. `Add Project...` ->
   OpenProject; the module validates and adds to `ProjectRegistry`.
6. **Listing is lazy and shallow.** Only immediate children are enumerated (via the source); navigation
   recomputes on location change. No recursive scans on the hot path.
7. **Pluggable source, not filesystem-specific.** `FileBrowserModel` is backed by an `IFileBrowserSource`
   (`Root/Parent/Join/IsDirectory/List/Places`). A `FileSystemSource` is the default; future asset sources
   list asset-database nodes and fill `FileBrowserEntry::typeId`, so the same dialog becomes the asset
   picker without changing the UI or the model. Locations are opaque strings owned by the source.
8. **Filters are first-class and support asset types.** `FileBrowserFilter { label, extensions[],
   assetTypes[] }` matches by extension and/or asset type id; the dialog exposes a filter selector
   (a dropdown that also lists "All Files"). `SELECT_DIRECTORY` always lists directories only.
9. **Blender/UE-style layout.** Sidebar of places (Home/drives), a toolbar (up + location), a columned
   list (Name/Type), and a footer with filter + name + Open/Cancel. Layout rectangles are exposed so hosts
   and tests target regions without duplicating constants.
10. **Shared headless text editor.** The Name field is driven by `TextEditState`
   (`editor/core/text/TextEditState.h`): text + caret + anchor selection with `Insert/Erase/Move/SelectAll`
   and `OnKey/OnText`. It filters control characters on insert, so platform `WM_CHAR` codes for
   Backspace/Enter/Tab/Esc never reach the buffer. The dialog only renders it. Editing logic is not
   re-implemented per widget.
11. **One input convention.** The platform forwards the engine `ScanCode` enum; UI widgets use virtual-key
   codes. `SandboxModule` maps `ScanCode -> VK` and drops `WM_CHAR` control codes **once** before
   forwarding. Widgets (including `EditBox`/`ReflectedFormView`) must not reinterpret key codes.

## Risks / Trade-offs

- **[Modal input/focus]** the dialog must capture pointer/keyboard over the hub and restore focus on close;
  test open/cancel/accept headlessly where possible.
- **[Cross-platform paths]** use `std::filesystem`; tolerate non-ASCII paths (UTF-8) and missing/removed
  directories by surfacing an inline error and keeping the previous listing.
- **[Name/path editing]** a single editable location field with basic validation; full path parsing is
  best-effort.
- **[Retiring the native picker]** `Add Project` behavior changes; ensure the engine-drawn path covers the
  same validation/status feedback.

## Migration Plan

Increments: (1) `core` `FileBrowserModel` + tests; (2) `FileBrowserDialog` widget; (3) shell hosting API;
(4) Project Manager New/Add wiring in `SandboxModule`; (5) remove the Win32 open dialog; (6) docs. Rollback:
revert; the Project Manager falls back to the previous paths if the dialog is absent.
