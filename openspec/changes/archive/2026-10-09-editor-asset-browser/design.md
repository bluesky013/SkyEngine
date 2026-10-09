## Context

The non-Qt editor (`engine/sandbox`) splits into a headless `EditorCore` (static lib, links only
`Core`/`Framework`; no `ui`/`aurora`/Qt in public headers) and an `EditorShell` built on the
in-house `sky::ui` toolkit. Panels are registered through `PanelRegistry` +
`RegisterBuiltinPanelViews` and composed into `LayoutModel`. The only asset surface today is
`IEditorAssetCatalog` (`editor/core/asset/EditorAssetCatalog.h`), a flat type-filtered lookup used by
asset-typed property fields; the legacy Qt `AssetBrowserWidget`/`AssetWidget` live in the unbuilt
`engine/editor` module.

The asset namespace is a **logical vpath owned by `AssetDataBase`**: sources are registered against
the ordered mounts (writable workspace, read-only engine) and persisted in per-directory manifests;
`AssetDataBase::RebuildCacheFromScan` discovers sources by builder-known extensions. There is no
general filesystem directory listing, and the browser should not introduce one — the tree must be a
view over the registered sources, not the raw disk.

One prerequisite is broken: `IEditorAssetCatalog.cpp` uses a function-local
`static AssetDatabaseCatalog` plus a global pointer. `EditorCore` is a static lib linked into each
module DLL, so each DLL gets its own instance — the anti-pattern the project's cross-DLL singleton
rule forbids.

The `asset-pipeline` change has landed the authoring-side framework APIs this browser consumes:
per-directory manifests and the mount namespace (`AssetDataBase`), a source catalog
(`ISourceCatalog`), an asset dependency graph (`IAssetDependencyProvider`), cook configuration
(`CookConfig`), the product/`AssetManager` loading layer, and the `IAssetEvent::OnAssetBuildFinished`
completion event.

Reference engines: Unity's **Project window**, Unreal's **Content Browser**, Godot's **FileSystem
dock** — all present a two-pane tree + list with breadcrumb, search, and context actions. This design
borrows that shape while keeping the model UI-free and constrained to the in-house toolkit's actual
capabilities (list + scroll; no grid element, no drag system).

## Goals / Non-Goals

**Goals:**
- A headless, cross-DLL `Singleton<EditorAssetCatalog>` in `EditorCore` whose tree is derived from the
  asset database's registered sources (logical paths grouped by mount).
- A shell asset-browser panel: tree + list, breadcrumb, search, type filter, sort, multi-select.
- Actions: New Asset, Import, Rename, Move, Duplicate, Delete, Cook/Build, Reimport, Copy Reference,
  Show in Explorer, Find References, Refresh; read-only mounts never mutated.
- Asset-open delegation through a toolkit-independent registry, with asset data loaded via the
  product loader (not source).
- Cook/product state badges and a detail pane (metadata, effective cook config, dependencies).

**Non-Goals:**
- Raw-disk browsing: unregistered files, empty folders, and folder-creation UI are out of scope; the
  tree shows the registered asset namespace. No `IFileSystem::ListDir` is added.
- Grid view (only `ListView`/`ScrollView` exist) — deferred.
- Drag-and-drop reference assignment and drop-file import (no `sky::ui` drag subsystem) — deferred;
  use the existing asset-typed field picker and the Import action instead.
- Rendered thumbnails, collections/favorites, source-control integration, virtual/pak/DLC mounts.
- The standalone builder-side `AssetTool` frontend (`asset-pipeline` task 10.6a) — a separate
  deliverable; this change is the in-editor counterpart.

## Decisions

### D1. Virtual-path tree derived from the asset database

The catalog builds a folder tree from the registered sources' logical paths: each source's directory
prefixes become folder nodes, its filename becomes a leaf item; roots are grouped by owning mount
(workspace, engine). No filesystem directory listing is used; the tree is a pure function of
`AssetDataBase`'s source set, so it cannot drift from the pipeline's identity model and works
uniformly for any mount (native, future pak/DLC). Unregistered raw files and empty folders are not
shown (non-goal); importing a file gives it identity and it then appears. Manifest/sidecar files
(such as the per-directory `assets.jsonl`) are not sources and are never listed; an item is shown
only when it has a known derived type.

### D2. Lock-safe source enumeration

`AssetDataBase::GetSources()` returns the id map by reference without synchronization, and a build
thread mutates it. The catalog MUST NOT read it unlocked. Add a small read-only accessor to
`AssetDataBase` (e.g. `ForEachSource(callback)` or `CollectSources()` copying under the existing
`assetMutex`, alongside `Gather`). The catalog builds its tree/snapshots from that.

### D3. Two-layer split: headless catalog model, UI panel

`EditorCore` owns `EditorAssetCatalog` (render/toolkit-independent) exposing a lazy vpath tree and
item lists; `AssetBrowserPanel` (in `engine/sandbox/shell`) renders it with `sky::ui` and drives it
through actions. Rationale: matches the `EditorCore` constraint and makes the model unit-testable
headless, mirroring `SelectionService` / `PropertyModel` / `FileBrowser`.

### D4. Cross-DLL singleton catalog

`EditorAssetCatalog` SHALL be `Singleton<EditorAssetCatalog>` (instance in `Environment`) so the
module that registers/constructs it and the shell panels in other DLLs see one instance. The existing
`IEditorAssetCatalog` default implementation SHALL delegate to this singleton instead of a
function-local static; the injectable `SetEditorAssetCatalog` seam is retained for tests, with the
singleton as the default. This fixes the current cross-DLL defect rather than reproducing it.

### D5. Data source: database for everything

- Tree structure, identity, name, and path come from `AssetDataBase` (registered sources, manifests);
  type is the derived `AssetTypeId` from the builder registry, never a persisted category.
- Product/cook state comes from the build event cache (no product re-read, no filesystem polling).
- Dependencies come from `IAssetDependencyProvider`; the effective cook config from `CookConfig`
  (asset override × project preset × platform).
- Snapshots are copied under the asset database lock and cached; they are invalidated on mutation
  (`ImportAsset`/`MoveAsset`/`DuplicateAsset`/`RemoveAsset` through the catalog), on build events, and
  on explicit refresh. Entries are ordered deterministically (name-sorted).

### D6. Item identity and selection

Each item is keyed by UUID for references and by logical path for placement. Browser selection
publishes to `SelectionService` as `SelectionType::ASSET` (id = `Uuid`), so outliner/inspector/
selection consumers work unchanged; the inspector reference-field picker already lists assignable
assets through the catalog.

### D7. Commands through the action registry

All actions are `EditorActionRegistry` entries (`asset.import`, `asset.new`, `asset.rename`,
`asset.move`, `asset.duplicate`, `asset.delete`, `asset.cook`, `asset.reimport`,
`asset.copyReference`, `asset.showInExplorer`, `asset.findReferences`, `asset.refresh`). Enable rules
derive from the current selection and mount writability, so toolbar, menus, shortcuts, and the panel
context menu share one implementation. Because there is no generic popup API, the panel draws its own
context popup (precedent: `MenuBar` / `FileBrowserDialog`) built from the same action set.

### D8. Mutation ownership and read-only mounts

Import/Rename/Move/Duplicate/Delete delegate to `AssetDataBase::{ImportAsset, MoveAsset,
DuplicateAsset, RemoveAsset}` (UUID preserved on move, regenerated on duplicate) and are offered only
when the owning mount is writable. Engine (read-only) mounts expose view/cook/copy-reference only.
A newly imported/created asset appears in the tree because it becomes a registered source.

### D9. Cook trigger and status

Cook/Build/Reimport invoke `AssetDataBase::BuildAllTargets` / the configured target through the same
`ICookRunner` path the loader uses (in-process or out-of-process). The catalog subscribes to
`IAssetEvent::OnAssetBuildFinished` and updates per-UUID state (cooking → ready/failed), surfacing
`AssetBuildResult::error` on failure. Cooks never run on the UI/loader pool and `BlockUntilLoaded`
is never called on the asset pool.

### D10. Asset open extension point

Add a minimal, toolkit-independent asset-editor registry (id/type → open callback) registered through
the editor extension mechanism. Double-click resolves the type's editor and invokes it; when none is
registered it is a no-op (no error). The editor must load the asset's **data** through the
product-based `AssetManager` path (on-demand cook, LOADING → LOADED) and MUST NOT read source files
(consistent with `asset-pipeline` D3).

### D11. Refresh model

The catalog is invalidated on a mutation performed through the browser, on a build-finished event,
and on explicit Refresh. No filesystem watcher is introduced (deferred); external changes are
reconciled by Refresh (which re-reads the database scan).

### D12. UI toolkit and filters

Two panes: a folder tree (mount-grouped vpath, lazy expansion) and a `ListView` item view with
breadcrumb, search box, type filter, and sort. Rendered with `sky::ui` (`UiDraw`/`UiSkin`); the type
filter reuses the `editor-file-browser` extension/asset-type filter model. A detail pane shows the
selected asset's metadata, effective cook config, and dependencies. The panel registers as `assets`
and is added to the default layout in the **bottom dock** (tabbed with the Output Log), matching
Unity/Godot/O3DE which place the asset browser at the bottom.

### D13. Reserved thumbnail seam

Real thumbnail rendering (decode/load a product image, cache a GPU texture) is deferred, but the item
view is designed so it can be plugged in without changing the panel contract: define an optional,
toolkit-independent `IAssetThumbnailProvider` (type/UUID → image handle/async request) registered
through the editor extension mechanism. When no provider is registered, or it has no image yet, the
item renders a type-derived icon. The item view queries the provider through the seam only; it does
not itself load products or touches the renderer. This keeps thumbnails out of the core model and out
of v1 scope while reserving the integration point.

## Risks / Trade-offs

- **No raw disk / empty folders** → deliberate: one source of truth (the asset database). Importing
  or creating an asset makes it visible; raw file management stays out of the browser.
- **`GetSources` unlocked** → never read directly; add a locked read-only accessor and snapshot.
- **Listing cost on large projects** → tree built from one cached snapshot, rebuilt only on
  invalidation.
- **No grid / no DnD** → list + picker cover the core workflow; grid and DnD are explicit
  follow-ups, so the specs do not promise unavailable primitives.
- **Asset open loads products, not source** → on-demand cook path shared with the loader; a missing
  product surfaces as LOADING/FAILED, never a source read.
- **Read-only engine mounts** → action enablement + centralized `AssetDataBase` mutation.
- **Default-layout change** → saved layouts without `assets` are handled by the existing
  unknown-panel skip; a View toggle is added.

## Migration Plan

1. Add the read-only, lock-safe source enumeration accessor to `AssetDataBase` + unit test.
2. Add the headless `Singleton<EditorAssetCatalog>` (vpath tree from the accessor, state from the
   build event, deps/config queries, observers); re-point `IEditorAssetCatalog` at it.
3. Add catalog unit tests (vpath tree, filter/search, type, read-only gating, deps, state, notify,
   no `ui`/`aurora`/Qt includes).
4. Add the asset-editor registry and wire open through the product loader.
5. Add `AssetBrowserPanel` (list baseline) + detail pane; register the built-in view and default
   layout; wire `SelectionService`.
6. Register the asset actions in `SandboxModule` with enable rules and the context menu.
7. Wire cook trigger + status, import (via the file browser dialog).
8. Update the affected specs (`editor-ui-shell`, `editor-command-registry`) and `docs/`.

Rollback: hide the panel and its `assets` layout entry; the catalog and the accessor can remain
unused with no framework behavior change.
