> Status: **blocked-on-prereq** — depends on `per-asset-cook-settings` (reflected `GetCookSettings` /
> `ReflectedFormView` baseline) and `editor-asset-browser` (catalog, panel, double-click open handler).
> Archive after those.

## Why

Double-clicking an asset in the browser currently reaches the open extension point
(`EditorAssetEditorRegistry` -> `IEditorAssetEditor::Open`), but no implementation exists, so nothing opens.
Authors need a proper per-asset viewer to inspect/edit an asset's cook settings without the cramped details
strip, and a place to later host a rendered preview. This adds a reusable **asset viewer widget** opened on
double-click, with a **reserved preview seam** (no preview rendering yet).

## What Changes

- Add a reusable **`AssetViewerWidget`** (shell): a floating overlay with a header (name/type/path), a
  **reserved preview region**, and the asset's **cook settings** rendered by the generic
  `ReflectedFormView` (the config-UI convention from `AGENTS.md`).
- Add an **`IAssetPreviewProvider`** seam (editor core, mirroring `IAssetThumbnailProvider`) with a
  cross-DLL registry; the widget queries it for the preview region and falls back to a placeholder. No
  preview rendering is implemented.
- Host the widget in `EditorShell` (like `FileBrowserDialog`) and route **double-click** (and the
  `asset.open` action) to `EditorShell::OpenAssetViewer(uuid)`.
- Non-goals: actual preview rendering, multi-asset tabs, docking the viewer as a panel.

## Capabilities

### New Capabilities

- `editor-asset-viewer`: the reusable asset viewer widget (header + reserved preview region + reflected
  cook-settings form), the preview-provider seam, and its open/close behavior.

### Modified Capabilities

- `editor-ui-shell`: the shell hosts the viewer overlay, exposes `OpenAssetViewer`, and routes double-click
  / `asset.open` to it.

## Impact

- Editor core: `editor/core/asset/AssetPreviewProvider.{h,cpp}` (new `IAssetPreviewProvider` +
  `AssetPreviewProviderRegistry`).
- Editor shell: `editor/shell/AssetViewerWidget.{h,cpp}` (new), `EditorShell.{h,cpp}` (own/route/layout the
  viewer), `editor/shell/panels/AssetBrowserPanel` (unchanged; double-click already calls the open handler).
- Module: `SandboxModule.cpp` (`SetAssetOpenHandler` -> `shell.OpenAssetViewer`).
- Tests: `EditorCoreTest` (preview registry seam), `EditorShellTest` (double-click opens + binds the viewer;
  Esc closes; no provider -> placeholder).
- Prerequisites: `per-asset-cook-settings`, `editor-asset-browser` (both pending archive).
