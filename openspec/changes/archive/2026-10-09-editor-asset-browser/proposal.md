## Why

The non-Qt sandbox editor has no asset browser: the legacy Qt `AssetBrowserWidget` lives in the
unbuilt `engine/editor` module, and the only asset UI in the new shell is the inline catalog used by
asset-typed property fields (`IEditorAssetCatalog`). Authors cannot browse the project/engine asset
namespace, see cook state, inspect the effective cook configuration, trigger cooks, or
import/move/rename/delete/duplicate assets.

Browsing must follow the asset pipeline's model: the asset namespace is the **logical vpath owned by
`AssetDataBase`** (mounts + manifests), not the raw disk. The catalog therefore derives its folder
tree from the registered sources' logical paths, so there is a single source of truth and no
OS/filesystem directory-listing dependency.

One prerequisite is currently broken: `IEditorAssetCatalog` is implemented as a function-local static
plus a global pointer, which violates the cross-DLL singleton rule (every module DLL would see its
own instance). This change fixes that while adding the browser.

## What Changes

- Add a **headless, cross-DLL `Singleton<EditorAssetCatalog>`** in `EditorCore`
  (`editor/core/asset`) that builds a **virtual-path (vpath) tree from `AssetDataBase`'s registered
  sources** (logical path prefixes, grouped by mount) and exposes per-asset type, name, source path,
  cook targets, product state, and dependency edges (backed by `AssetDataBase`, `ISourceCatalog`,
  `IAssetDependencyProvider`, `CookConfig`). It stays render-/toolkit-independent (no `ui/`,
  `aurora/`, or Qt) and replaces the function-static default of the existing `IEditorAssetCatalog`,
  which becomes a query over it.
- Add a small **read-only, lock-safe enumeration accessor** to `AssetDataBase` (e.g. an iteration or
  snapshot of the registered sources) so the catalog can build its tree without reading the
  unprotected source map. No filesystem `ListDir` and no raw-disk scanning are introduced.
- Add an **asset browser panel** to the shell: dual-pane (folder tree + item list) with a breadcrumb,
  search filter, type filter, sort, and multi-select. **List view is the required baseline; a grid
  view is deferred.**
- **Context actions** via the action registry: New Asset, Import, Rename, Move, Duplicate, Delete,
  Cook/Build, Reimport, Copy Reference, Show in Explorer, Find References, Refresh. Mutating actions
  are gated on the owning mount being writable (engine mounts are read-only).
- **Asset open extension point**: double-click delegates to a toolkit-independent asset-editor
  registry; the asset's *data* loads through the product loader (on-demand cook, LOADING → LOADED),
  **never from source files**.
- **Cook state badges** per asset (not-cooked / cooking / ready / failed) driven by the
  `OnAssetBuildFinished` event, and a **detail pane** showing metadata, effective cook config, and
  dependencies/dependents.
- **Item presentation**: the browser lists only registered assets with a known derived type; manifest
  and sidecar files (e.g. `assets.jsonl`) are never shown. Each item renders a type-derived icon, and
  a **thumbnail-provider seam is reserved** (an optional provider keyed by type; the icon is the
  fallback) so real thumbnails can be plugged in later.
- **Deferred** (explicit non-goals): grid view, drag-and-drop of assets onto the viewport/inspector
  and drop-file import, rendered thumbnails (only the seam + icon fallback is in scope), raw/
  unregistered files and empty folders in the tree, folder creation UI, and the standalone
  builder-side `AssetTool` frontend. The `asset-pipeline` task 10.6a (a separate `AssetTool` app)
  remains its own deliverable; this change is the in-editor counterpart.

## Capabilities

### New Capabilities

- `editor-asset-catalog`: the headless, toolkit-independent, cross-DLL singleton catalog that derives
  a virtual-path tree from the asset database's registered sources, with type/name/cook-target/
  product-state queries, dependency lookup, effective cook config, mount writability, and change
  notification.
- `editor-asset-browser`: the asset-browser panel UI and interactions (tree + list, filter/search,
  multi-select, context actions, asset-open delegation, cook-status display, detail pane).

### Modified Capabilities

- `editor-ui-shell`: registers the built-in `assets` panel view and its default layout placement.
- `editor-command-registry`: registers the asset-browser actions (import/new/rename/move/duplicate/
  delete/cook/open/copy-reference) with selection- and mount-driven enable rules.

## Impact

- `engine/sandbox/core/include|src/editor/core/asset`: add the singleton `EditorAssetCatalog` and its
  types (`EditorAssetItem`, `EditorAssetFolder`, `CookState`, effective-config snapshot); re-point the
  existing `IEditorAssetCatalog` at the singleton.
- `engine/sandbox/core/include|src/editor/core/asset`: add a minimal asset-editor registry used by
  the browser's open action.
- `engine/framework/.../AssetDataBase.*`: add a read-only, lock-safe accessor to enumerate the
  registered sources (the catalog's tree/metadata source).
- `engine/sandbox/shell/.../panels` + `EditorShell.cpp`: new `AssetBrowserPanel` (list baseline),
  registered through `RegisterBuiltinPanelViews`; default layout gains `assets`.
- `engine/sandbox/module/SandboxModule.cpp`: wire the panel, register asset actions, and subscribe to
  the build event.
- `engine/framework`: consumes `AssetDataBase` (mounts/manifests, `Gather`, `ImportAsset`,
  `MoveAsset`, `DuplicateAsset`, `RemoveAsset`, `BuildAllTargets`), `ISourceCatalog`,
  `IAssetDependencyProvider`, `CookConfig`, and `IAssetEvent::OnAssetBuildFinished`.
- Tests: `EditorCoreTest` (catalog), `EditorShellTest` (panel).
- Non-goals (deferred): grid view, drag-and-drop reference assignment, drop-file import, rendered
  thumbnails, raw/unregistered files and empty folders, folder-creation UI, collections/favorites,
  source-control integration, virtual/pak/DLC mounts, and the standalone builder-side `AssetTool`
  frontend.
