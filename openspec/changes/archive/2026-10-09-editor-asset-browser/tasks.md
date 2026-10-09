## 1. Asset database read access

- [x] 1.1 Add a read-only, lock-safe enumeration accessor to `AssetDataBase`
  (`engine/framework/include|src/framework/asset/AssetDataBase.*`), e.g. `ForEachSource(cb)` or
  `CollectSources()` copying under `assetMutex`; add a unit test that it is consistent under mutation.

## 2. Headless asset catalog (EditorCore)

- [x] 2.1 Add `EditorAssetFolder`/`EditorAssetItem` (UUID, name, logical path, type, cook state,
  error, writable) and a lazy vpath tree API to `engine/sandbox/core/include/editor/core/asset`,
  derived from the registered source paths grouped by mount, with no `SourceAssetBundle` dependency.
- [x] 2.2 Implement the catalog via the `AssetDataBase` accessor: list folders/items, lookup by UUID
  and path, type-filtered list, and derived `AssetTypeId`.
- [x] 2.3 Make `EditorAssetCatalog` a `Singleton<EditorAssetCatalog>` (environment-held) and
  re-point the existing `IEditorAssetCatalog` default implementation at it (remove the
  function-local static + global pointer); keep `SetEditorAssetCatalog` as the test seam.
- [x] 2.4 Add per-UUID cook/product state (not-cooked/cooking/ready/failed + error), driven by cook
  triggers and `IAssetEvent::OnAssetBuildFinished` (no filesystem polling).
- [x] 2.5 Expose effective cook configuration (asset override x project preset x platform), declared
  targets, and `IAssetDependencyProvider` forward/reverse queries; compute the effective config from
  `CookConfig`.
- [x] 2.6 Expose mount writability per item; add observer registration + `Refresh()`, invalidated by
  framework mutations (import/move/duplicate/delete), build events, and explicit refresh; keep
  snapshots locked and deterministically ordered.

## 3. Catalog tests

- [x] 3.1 Add headless `EditorCoreTest` cases: vpath tree from registered sources, empty folder
  absent, item metadata and derived type, lookup by UUID/path, type filter, deterministic order.
- [x] 3.2 Add tests for cook/product state transitions on build-finished success/failure, effective
  cook config resolution, dependency forward/reverse, mount writability, and observer/refresh
  notification.
- [x] 3.3 Add a test that the catalog is resolved as one instance across translation units/modules
  (singleton), and a config check that no public asset-catalog header includes `ui/`, `aurora/`, or a
  Qt header.
- [x] 3.4 Add tests that manifest/sidecar files (`assets.jsonl`) and unknown-extension paths are not
  listed as items or folders.

## 4. Asset open extension point

- [x] 4.1 Add a minimal toolkit-independent asset-editor registry (id/type -> open callback)
  registered through the editor extension mechanism.
- [x] 4.2 Wire the open action to load asset data through the product-based `AssetManager` path
  (on-demand cook, LOADING -> LOADED), never reading source files; a missing editor is a no-op.

## 5. Asset browser panel (shell)

- [x] 5.1 Add `AssetBrowserPanel` + widgets in `engine/sandbox/shell/.../panels` rendering the
  catalog with `sky::ui`: folder tree, item list (`ListView`), breadcrumb, search, type filter, sort.
- [x] 5.2 Register the built-in `assets` panel view in `EditorShell::RegisterBuiltinPanelViews` and
  add it to the default layout (plus a View toggle).
- [x] 5.3 Publish selection to `SelectionService` as asset selections (UUID) and react to external
  selection changes.
- [x] 5.4 Add a detail pane for the selected asset (metadata, effective cook config, dependencies and
  dependents).
- [x] 5.5 Reserve the thumbnail seam: define the optional `IAssetThumbnailProvider` extension point
  and render a type-derived icon as the fallback (no product loading in the panel).

## 6. Actions and context menu

- [x] 6.1 Register the asset actions in `EditorActionRegistry` (`asset.import`, `asset.new`,
  `asset.rename`, `asset.move`, `asset.duplicate`, `asset.delete`, `asset.cook`, `asset.reimport`,
  `asset.copyReference`, `asset.showInExplorer`, `asset.findReferences`, `asset.refresh`,
  `asset.open`) with selection/mount enable predicates, wired in `SandboxModule`.
- [x] 6.2 Implement mutation actions through `AssetDataBase::{ImportAsset, MoveAsset, DuplicateAsset,
  RemoveAsset}` and gate them on writable mounts; refresh affected folders.
- [x] 6.3 Render the panel context menu from the registered actions (own popup, no generic API) and
  add New Asset using the registered asset creators.

## 7. Cook and import

- [x] 7.1 Implement Cook/Build/Reimport actions through `AssetDataBase::BuildAllTargets` / the
  configured cook target and the `ICookRunner` path; show cooking -> ready/failed and surface the
  error.
- [x] 7.2 Implement the Import action via the existing `FileBrowserDialog`, refusing read-only
  folders.

## 8. Verification and docs

- [x] 8.1 Add a shell test that the `assets` panel is registered, in the default layout, and updates
  on catalog notification.
- [x] 8.2 Add an integration check that import/move/duplicate/delete preserve or reassign identity as
  specified and leave references resolvable.
- [x] 8.3 Update the affected `docs/` (editor asset browser usage, cook state, effective cook config)
  and the `editor-ui-shell` / `editor-command-registry` spec references.
