## Why

Creating a new project currently hard-codes a location (the user projects dir) and adding an existing
project shells out to a raw Win32 `GetOpenFileNameW`. There is no reusable, engine-drawn way to pick a
**directory**, a **project file**, or a **general file**. Other engines expose a platform file-dialog
abstraction plus an engine content browser; the editor needs the same so **New Project** can choose a target
directory and **Add / Open Project** can choose a `*.skyproj`.

## What Changes

- Add a reusable, engine-drawn, modal **`FileBrowserDialog`** (`sky::ui`) with modes **OpenFile**,
  **OpenProject**, and **SelectDirectory**, plus extension filters, an editable path/location field, a
  directory listing with navigation, a file-name field, and OK/Cancel.
- **Project Manager**: **New Project...** opens **SelectDirectory** (choose the parent directory, then a
  project name); **Add Project...** opens **OpenProject** filtered to `*.skyproj`; a generic **Open File**
  entry uses OpenFile.
- Retire the ad-hoc Win32 `GetOpenFileNameW` in `SandboxModule`; the shell hosts the dialog and the module
  supplies the starting directory and consumes the result.
- **BREAKING** for the current auto-named "MyProject" path (New now requires choosing a directory).

## Capabilities

### New Capabilities
- `editor-file-browser`: a reusable modal file/directory chooser dialog with OpenFile / OpenProject /
  SelectDirectory modes, extension filters, and a navigation/listing model.

### Modified Capabilities
- `editor-application`: the Project Manager hub New/Add/Open flows use the file-browser dialog (directory
  chooser for New, project chooser for Add/Open) instead of a hard-coded path or a raw native picker.

## Impact

- `engine/sandbox/shell`: new `FileBrowserDialog` widget + a UI-free listing/navigation model; hosted by the
  shell (like the Preferences dialog); the Project Manager hooks (`onAdd`/`onNew`) already exist.
- `engine/sandbox/module`: `SandboxModule` New/Add wiring; remove the direct Win32 open dialog; supply the
  initial directory and consume the chosen path.
- `engine/framework` (optional): a directory-capable native dialog (`ShowSelectFolderDialog`) as a fallback
  and for non-Windows, mirroring the existing `ShowOpenFileDialog`/`ShowSaveFileDialog` seam.
- Tests: listing/navigation/filter model (headless) + a shell dialog smoke test.
- Non-goal (deferred): full content browser / asset thumbnails (`editor-content-browser`), multi-select,
  recent-locations, and per-project virtual file systems.
