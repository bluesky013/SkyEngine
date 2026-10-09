> Prerequisites: `per-asset-cook-settings` and `editor-asset-browser` (both complete, pending archive). No
> preview rendering is implemented (seam only).

## 1. Preview seam (editor core)

- [x] 1.1 Add `IAssetPreviewProvider { bool GetPreview(const Uuid&, const std::string &type) }` +
  `AssetPreviewProviderRegistry : Singleton<...>` (`SetProvider`/`GetProvider`), mirroring
  `IAssetThumbnailProvider` (`editor/core/asset/AssetPreviewProvider.{h,cpp}`).
- [x] 1.2 `EditorCoreTest`: register/query/clear the provider (seam test).

## 2. Asset viewer widget (editor shell)

- [x] 2.1 Add `AssetViewerWidget : ModalDialog` (`editor/shell/AssetViewerWidget.{h,cpp}`): ctor takes
  `UITextSystem*`; `SetCatalog`; `Open(uuid)` loads the item + cook settings, seeds the target, binds the
  embedded `ReflectedFormView` (header + preview region + form + close).
- [x] 2.2 `OnPaint`: backdrop + centered panel; header (name/type/path); reserved preview region (query the
  preview provider; placeholder when none); position the form child; close button.
- [x] 2.3 Input: forward pointer (inside form bounds) / key / text to the form child first; handle the close
  button and Escape; keep the target-selector click.
- [x] 2.4 Commit edits through `EditorAssetCatalog::ApplyCookSettings` on form change.

## 3. Shell hosting and open routing

- [x] 3.1 `EditorShell`: own an `AssetViewerWidget` (like `FileBrowserDialog`), expose
  `OpenAssetViewer(uuid)`, include it in `ActiveModal()`/`Layout`, and `IsAssetViewerOpen()`.
- [x] 3.2 `SandboxModule`: `SetAssetOpenHandler` routes to `shell.OpenAssetViewer(id)` when no type-specific
  `IEditorAssetEditor` is registered (type-specific editor wins).
- [x] 3.3 `EditorShellTest`: double-click/open opens the viewer and binds the form; Escape closes; no provider
  -> placeholder path exercised.

## 4. Verification

- [x] 4.1 Run `FrameworkTest` / `AuroraCookTest` / `EditorCoreTest` / `EditorShellTest`.
- [x] 4.2 Update `docs/editor/asset-browser.md` (viewer widget + preview seam).

## 5. Resizable viewer

- [x] 5.1 Bottom-right resize grip (hover feedback, drag) with min/clamp; `CenteredPanel` uses the stored
  size.

## 6. Per-asset-type customization + cleanup

- [x] 6.1 `AssetViewProviderRegistry` + `IAssetViewProvider` (shell, cross-DLL) supplying custom
  content/preview widgets per asset type; the viewer falls back to defaults.
- [x] 6.2 Remove the duplicate details-pane reflected form (single config editor = the viewer); details pane
  is info + target selector + hint.
- [x] 6.3 Re-run tests (`EditorShellTest` viewer open/close, catalog edit/reset).
