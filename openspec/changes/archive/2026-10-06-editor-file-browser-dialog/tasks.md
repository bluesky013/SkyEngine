## 0. Abstraction (source + filters) and UX redesign

- [x] 0.1 Add `IFileBrowserSource` (Root/Parent/Join/IsDirectory/List/Places) + `FileSystemSource`; `FileBrowserEntry{name,path,isDirectory,typeId}`; `FileBrowserFilter{label,extensions[],assetTypes[]}`; `FileBrowserPlace`; `request.places`.
- [x] 0.2 Back `FileBrowserModel` with the source; add `SetActiveFilter`/`GetFilterLabel` and extension+asset-type matching; `SELECT_DIRECTORY` lists directories only.
- [x] 0.3 Redesign `FileBrowserDialog` to a Blender/UE layout: places sidebar, toolbar (up + location), Name/Type list, filter dropdown + name + Open/Cancel; expose layout rects for hosts/tests.
- [x] 0.4 Tests: model source/filter/directory-only; dialog filter-popup switch to "All Files"; shell-hosted modal.

## 0b. Shared text editing + input unification

- [x] 0b.1 Add headless `TextEditState` (`editor/core/text/`) with caret + anchor selection and `Insert/Erase/Move/SelectAll`, `OnKey` (virtual-key codes), `OnText`, and control-character filtering; tests in `TextEditStateTest`.
- [x] 0b.2 Refactor `FileBrowserDialog`'s Name field to use `TextEditState`; drop the inline caret/selection/insert/erase code.
- [x] 0b.3 Unify input in `SandboxModule`: map platform `ScanCode -> virtual-key` and filter `WM_CHAR` control codes once, so all UI widgets agree.

## 1. Core: headless file-browser model

- [x] 1.1 Add `editor/core/filebrowser/FileBrowserTypes.h`: `FileBrowserMode` (OpenFile/OpenProject/SelectDirectory), `FileBrowserFilter { label, extensions }`, `FileBrowserRequest`, `FileBrowserResult`.
- [x] 1.2 Add `FileBrowserModel`: current directory, entry listing (name / isDirectory), navigate to a child, navigate to parent, set directory by path, extension-filter matching, selection, editable name/location, and error state. Headless, `std::filesystem`-only, no UI types.
- [x] 1.3 Tests: listing + extension filter, navigate into/parent, invalid directory keeps previous listing, SelectDirectory name combination.

## 2. Shell: FileBrowserDialog widget

- [x] 2.1 Add a `FileBrowserDialog` `sky::ui::UIElement`: dim backdrop + centered panel with a location/path field, a directory listing, a file-name field, and OK/Cancel.
- [x] 2.2 Modal input: while open, capture pointer/keyboard; Esc = Cancel, Enter = OK.
- [x] 2.3 Render with the `uidraw` palette (rows, selection, hover/up/button states) consistent with the hub.
- [x] 2.4 Headless `EditorShellTest` smoke: open/Esc-cancel, Enter-accept (SelectDirectory), Cancel button, double-click navigate into a directory + Up, OK accepts a project file. (Tests the dialog element directly; it is hosted by the hub rather than `EditorShell`.)

## 3. Hosting and Project Manager wiring

- [x] 3.1 Add `EditorShell::OpenFileBrowser(FileBrowserRequest, callback)` (+ `IsFileBrowserOpen`): the shell owns one dialog added last in `Rebuild`, sizes it in `Layout`, routes pointer/key/text to it while open, and reopens it across rebuilds. Tested by `EditorShellTest.OpenFileBrowserHostsModal`.
- [x] 3.2 Wire the Project Manager hub: `onNew` -> SelectDirectory request; `onAdd` -> OpenProject request; feed results back via `FileBrowserDialog::SetOnResult`.
- [x] 3.3 Retire the direct Win32 `GetOpenFileNameW` in `SandboxModule`; validate and create/add from the dialog result, preserving status feedback.
- [x] 3.4 Add a New Folder action: `IFileBrowserSource::CreateDirectory` (+ `FileSystemSource`), `FileBrowserModel::CreateFolder` (unique name + reload + select), and a dialog toolbar button (write mode only); test in `FileBrowserModelTest`.
- [x] 3.5 Add rename + right-click context menu: `IFileBrowserSource::Rename` (+ `FileSystemSource`), `FileBrowserModel::RenameSelected`, dialog context menu (New Folder / Rename) and inline rename via the name field (New Folder enters rename mode); test in `FileBrowserModelTest`.

## 4. Optional platform fallback and docs

- [ ] 4.1 Optional: add `Platform::ShowSelectFolderDialog` (native directory picker) as a fallback seam, mirroring `ShowOpenFileDialog`/`ShowSaveFileDialog`.
- [x] 4.2 Update `docs/editor/editor-framework-design.md` / status doc with the file browser dialog section. (Added §3.10.)

## 5. Verification

- [x] 5.1 Build `SandboxEditor` + `EditorCoreTest` (FileBrowserModelTest 5/5) + `EditorShellTest` (FileBrowserDialogTest 5/5, suite 13/13). SandboxEditor links the dialog and runs.
- [x] 5.2 Manually verify: New Project chooses a directory and creates there; Add Project filters `*.skyproj` and adds; Cancel leaves the hub unchanged. (Verified.)
