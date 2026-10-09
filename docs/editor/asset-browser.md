---
title: "Editor Asset Browser"
description: "Design and status of the Sandbox editor asset browser: a virtual-path tree over the asset database, cook state, asset actions, and the reserved thumbnail seam."
updated: "2026-10-08"
---

## Goal

An **asset browser** panel in the non-Qt Sandbox editor: a folder tree plus an item list over the
authoring asset namespace, with cook state, asset actions (import/cook/delete/…), selection that
feeds the inspector, and a reserved seam for thumbnails. It must not reintroduce a second source of
truth (no raw filesystem listing) or the legacy Qt `AssetBrowserWidget`.

## Model (facts)

- The asset namespace is the **logical vpath owned by `AssetDataBase`** (ordered mounts + per-directory
  manifests). There is no general directory-listing primitive on `IFileSystem`; the browser derives
  its tree from the **registered sources' logical paths** (path prefixes become folders, the filename
  a leaf).
- **Mount provenance is a framework concept**: `AssetDataBase::GetMounts()` returns the ordered
  `AssetMount { id, displayName, writable }`, and each `AssetSourceInfo` carries its owning mount id.
  The catalog maps id → display/writable; it does **not** probe filesystems or hardcode mount names,
  so new mounts (pak/DLC) appear without editor changes.
- `EditorAssetCatalog` (`engine/sandbox/core/.../asset`) is a **cross-DLL `Singleton`** in
  `EditorCore`; it is the single service behind the panel and the asset-typed property fields
  (via `IEditorAssetCatalog`). It reads sources through `AssetDataBase::ForEachSource` (a lock-safe
  copy) and derives types from the builder registry (`AssetDataBase::GetType`); type is optional
  metadata, not a filter.
- `GetTree()` builds the whole nested virtual-path tree in **one pass** over the sources; the panel
  only flattens it per its expand state. The build listener is private (the public header does not
  expose the framework event interface).
- Only **registered sources** are shown; manifest/sidecar files (`assets.jsonl`) are never registered
  and so never appear. Directories with no registered assets are not represented (no empty folders).
- Cook state is tracked **per `(uuid, bundle)`** (aggregate + per-target) from
  `IAssetEvent::OnAssetBuildFinished` and cook triggers, never from filesystem polling.

## Panel

`AssetBrowserPanel` (`engine/sandbox/shell/.../panels`) is a `sky::ui` element implementing
`IPanelChrome`, registered as the built-in `assets` panel and added to the default layout in the
**bottom dock** (tabbed with the Output Log) — Unity, Godot and O3DE all place the asset browser at
the bottom.

- **Left**: a collapsible virtual-path tree (mount → subfolders), like the file browser; click a node
  to toggle expand and navigate.
- **Right**: the current folder's assets as thumbnail icon tiles (larger icon + centered name; a
  cook-state dot when not `not-cooked`), falling back to a type-tinted icon via the thumbnail seam.
- **Breadcrumb**: current logical path.
- **Detail** (bottom): selected asset's type, path, the active **platform**, and a **target selector**
  (click to cycle). Cook settings are **not** edited here — double-click opens the asset viewer (or a
  per-type provider), the single place cook settings are edited.
- Selection publishes to `SelectionService` as `SelectionType::ASSET` (id = UUID), so the outliner and
  inspector follow. Double-click invokes the open handler.

## Actions

Registered in `EditorActionRegistry` (so toolbar, menus, shortcuts, and the panel's own context
popup share one implementation), with enable rules from the selection and mount writability:

| id | behaviour | writable-gated |
|---|---|---|
| `asset.refresh` | re-read the database and notify views | no |
| `asset.open` | open via the registered asset editor | no |
| `asset.cook` | cook the selected asset across its resolved targets | no |
| `asset.copyReference` | copy/log the asset UUID | no |
| `asset.showInExplorer` | reveal the source file (via `Platform::RevealInFileExplorer`) | no |
| `asset.findReferences` | reserved (log stub) | no |
| `asset.import` | import an external file via the file browser dialog | yes |
| `asset.duplicate` | duplicate the selected asset (new UUID) | yes |
| `asset.delete` | delete the selected asset | yes |
| `asset.rename` / `asset.move` | start an inline rename/move edit (F2 also works) | yes |
| `asset.new` | reserved (log stub) | yes |

Mutation delegates to `AssetDataBase::{ImportAsset, MoveAsset, DuplicateAsset, RemoveAsset}`
(UUID preserved on move/rename, regenerated on duplicate). Engine mounts are read-only, so mutating
actions are disabled for them.

Rename/Move use an **inline edit** (the Rename/Move action or **F2**): the name field becomes an
editable box, Enter commits through `MoveAsset` (the typed text may be a name or a full logical path,
so the same field performs rename and move), Escape cancels.

## Cook targets, settings, and storage

- **Targets**: an asset's cook targets are its `cook.targets` override, else the project's named
  targets, else the **active platform's preset bundles** (`CookConfig::GetPresetBundles`; e.g.
  `{common, tex_pc}`). The editor sets the active platform (host, lower-cased) at startup and
  registers the project's product bundles from `configs/*_build_presets.json`.
- **Settings (generic reflected form)**: a builder declares a reflected cook-settings type via
  `AssetBuilder::GetSettingsType()` / `MakeSettings(bundle, override)` / `DiffSettings(bundle, edited)`
  (e.g. `AuroraImageBuilder` declares `ImageCookSettings { encode, srgb, quality, block, maxSize,
  generateMip }`). The details pane edits them **only** through the generic `ReflectedFormView` bound to
  `PropertyObject{obj, node}` with the global-preset baseline, so enums/bools/ints get their standard
  controls and edits are undoable (`CommandService`). Hand-rolled per-field controls are not used (see the
  config-UI rule in `AGENTS.md`).
- **Per-asset override** (global default + override): a source manifest entry may carry a sparse
  `cook.settings` map keyed by **target/bundle**; the effective settings are the global bundle preset
  overlaid key-by-key, so unset keys keep the global default:
  ```json
  { "file": "textures/hero.png", "id": "…",
    "cook": { "targets": ["common","tex_mobile"],
              "settings": { "tex_mobile": { "maxSize": 512, "generateMip": false } } } }
  ```
  The framework resolves this and passes the flattened per-target override to the builder via
  `AssetBuildRequest::settings` (both in-process and worker cook paths), so builders never read the
  manifest. The catalog persists edits with `AssetDataBase::SetCookJson` (writable mounts only,
  `file`/`id` preserved) via `SetCookSetting` / `ResetCookSetting` / `SetCookTargets`; mutations do
  **not** auto-cook (use Cook/Reimport). ASTC block sizes 4/6/8 are supported (`ImageCookSettings.block`).
- **Storage (framework invariant)**: products are `products/<bundle>/<uuid[0:2]>/<uuid>.bin` with
  `products/<bundle>/product.index` (logical path → uuid). The editor only reads/subscribes and
  queries product presence via `AssetManager::HasProduct`. Packaging (`PackageAssetBundle`/`.pak`) is a
  stub and out of scope.

## Asset open

`EditorAssetEditorRegistry` (`engine/sandbox/core/.../asset`) is a toolkit-independent registry of
`IEditorAssetEditor` (keyed by asset type). Opening resolves the type's editor and calls `Open`;
when none is registered it falls back to the generic **asset viewer widget**. An editor is expected to load
the asset's **data** through the product-based loader (on-demand cook, `LOADING → LOADED`), never from source
files.

## Asset viewer widget

`AssetViewerWidget` (`engine/sandbox/shell/.../AssetViewerWidget`) is a reusable **resizable** modal overlay
opened on asset **double-click** (and the `asset.open` action) when no type-specific editor is registered. It
shows the asset header (name/type/path), a **preview region** (left), and a **content region** (right;
default = **one reflected cook-settings form per resolved target**, so all targets are visible/editable at
once, scrollable). It commits edits via `EditorAssetCatalog::ApplyCookSettings`, resizes via a bottom-right
grip, and closes on Escape/close-button. `EditorShell` hosts it like the file browser (`OpenAssetViewer` /
`IsAssetViewerOpen`, included in modal routing + layout).

**Per-asset-type customization:** `AssetViewProviderRegistry` (`editor/shell/.../AssetViewProvider.h`, a
cross-DLL `Singleton`) maps an asset type to an `IAssetViewProvider` that can supply a custom content widget
(right) and/or preview widget (left); the viewer uses them in place of the defaults. This is the seam for
asset-specific viewers (mesh/material/terrain) without changing the generic widget.

The preview region (when no provider preview) uses the reserved **`IAssetPreviewProvider`** seam
(`AssetPreviewProviderRegistry`, a cross-DLL singleton mirroring `IAssetThumbnailProvider`): the widget
queries the provider and draws a placeholder when none is registered. **No preview rendering is
implemented** — a future renderer-backed provider targets a `ViewContentSource::ASSET` viewport.

## Reserved thumbnail seam

`IAssetThumbnailProvider` + `AssetThumbnailProviderRegistry` reserve the thumbnail integration point:
the panel queries the provider per item and falls back to a type-derived icon tint when none is
registered or it has no image. Real thumbnail rendering (decode product image, cache a GPU texture) is
deferred; the panel never loads products or touches the renderer.

## Deferred / non-goals

Grid view, drag-and-drop reference assignment, drop-file import, rendered thumbnails (only the seam +
icon fallback is implemented), raw/unregistered files and empty folders in the tree, folder-creation
UI, the New Asset creator-picker flow, and the standalone builder-side `AssetTool` frontend.

## Files

- Core: `engine/sandbox/core/{include,src}/editor/core/asset/{EditorAssetCatalog,EditorAssetEditor,
  AssetThumbnailProvider}.{h,cpp}`.
- Framework: `AssetDataBase::{AssetMount, GetMounts, ForEachSource, GetCookJson, SetCookJson,
  GetAbsoluteSourcePath}`, `AssetBuilder::{DescribeSettings, GetSettingsType, MakeSettings, DiffSettings,
  BuildSettingsOverride}`, `AssetBuilderManager::{GetBuilderSettings, GetBuilderSettingsType,
  MakeBuilderSettings, DiffBuilderSettings}`, `CookConfig::GetTargetSettings`, `AssetBuildRequest::settings`,
  `ReflectedFormView::Bind(object, baseline)`; `Platform::RevealInFileExplorer` (Windows/macOS backends).
- Shell: `editor/shell/panels/AssetBrowserPanel.{h,cpp}`, `EditorShell` (`SetAssetCatalog` /
  `SetAssetActionHandler` / `SetAssetOpenHandler` / `SetAssetRenameHandler`, built-in `assets` view).
- Module: `SandboxModule.cpp` (catalog + builder-manager wiring, asset actions, default layout).
- Tests: `engine/sandbox/test/EditorAssetCatalogTest.cpp`, `EditorShellTest.cpp`,
  `engine/test/framework/{AssetManagerTest,AssetCookOverrideTest,CookConfigTest}.cpp`,
  `engine/aurora/cook/test/CookPipelineTest.cpp`.
- Prerequisite / follow-on: the `asset-pipeline-layering-refactor` change (this layering); the
  `per-asset-cook-settings` change (per-asset override).

## Testing

`EditorCoreTest` covers: vpath tree from registered sources, empty-folder absence, sidecar/unknown
exclusion, metadata/type/lookup, cook-state transitions, mutation identity (move/duplicate/delete),
the editor registry, the thumbnail-provider seam, and the per-asset cook override read/write
(set/reset, effective display, persistence). `EditorShellTest` covers the details-pane edit + reset
flow. `FrameworkTest` covers `CookConfig::GetTargetSettings`, the build-request override plumbing, and
the manifest write API. `Aurora.CookTest` covers the image builder applying the override over the
bundle preset. `DefaultPanelsTest` asserts the `assets` panel is registered.
