## Context

`AssetBrowserPanel` already detects double-click and calls its `OpenHandler`; `EditorShell::SetAssetOpenHandler`
and `EditorAssetEditorRegistry` form the open extension point, but no editor/widget is registered, so nothing
opens. The just-landed `per-asset-cook-settings` change gives the catalog a reflected `GetCookSettings` and
`ReflectedFormView` an explicit baseline, and the config-UI rule (`AGENTS.md`) requires config editors to use
the reflected form. `FileBrowserDialog`/`ModalDialog` show the shell's overlay-dialog pattern (child of the
main `UIContext`, routed by `EditorShell::ActiveModal`).

## Goals / Non-Goals

**Goals:** a reusable asset viewer widget opened on double-click; cook settings edited via the reflected
form; a reserved preview region with a provider seam; no preview rendering yet.

**Non-Goals:** real preview rendering, multi-tab viewers, docking the viewer, editing asset *data* beyond
cook settings.

## Decisions

### D1. `AssetViewerWidget : ModalDialog`, shell-owned overlay

Reuse the `ModalDialog` overlay pattern (centered panel + backdrop + Esc), owned by `EditorShell` on the main
`UIContext` and included in `ActiveModal()`/`Layout`, exactly like `FileBrowserDialog`. This gives modal
pointer/key routing and dismissal for free.

- Rejected: a docked panel (needs layout/activation plumbing) and a bespoke non-modal overlay (needs new
  input routing). A modal viewer is the smallest correct step; docking can come later.

### D2. Cook settings via the reflected form

The widget embeds a `ReflectedFormView` child bound to `catalog->GetCookSettings(uuid, target)` with the
preset baseline (`ReflectedFormView::Bind(object, baseline)`), matching the config-UI rule. It keeps a
target selector row (click cycles targets) and commits edits through `catalog->ApplyCookSettings`.

### D3. Child input forwarding

`EditorShell::ActiveModal` routes input to the modal element only (no child hit-testing). `AssetViewerWidget`
therefore forwards pointer events whose point is inside the form's bounds to `form->OnPointerEvent`, and
forwards key/text to the form first, then handles its own Esc/close.

### D4. Reserved preview seam

Add `IAssetPreviewProvider { bool GetPreview(const Uuid&, const std::string &type) }` +
`AssetPreviewProviderRegistry : Singleton<...>` in editor core (mirroring `IAssetThumbnailProvider` and its
cross-DLL singleton rule). The widget draws a bordered "preview" region; if a provider is registered it draws
a "preview available" placeholder (no pixels yet), otherwise an empty placeholder. Real rendering is deferred
to a renderer-backed implementation (future `ViewContentSource::ASSET` viewport).

### D5. Open routing

`SandboxModule`'s `SetAssetOpenHandler` calls `shell.OpenAssetViewer(id)` (after `LoadAsset`). The
`IEditorAssetEditor` registry remains the type-specific override seam: if an editor is registered for the
type it still wins; otherwise the generic viewer opens.

### D6. Resizable viewer

The viewer is resizable: a bottom-right grip drag adjusts a stored panel size (clamped to a minimum and the
window), and `CenteredPanel` uses it. The grip is drawn with hover feedback; the modal already receives
MOVE/DOWN/UP events. Rejected: separate resize borders (overkill for v1).

### D7. Per-asset-type customization

`AssetViewProviderRegistry` (shell, cross-DLL `Singleton`) maps an asset **type** to an `IAssetViewProvider`
that can supply a custom content widget (right region) and/or preview widget (left region). The viewer asks
the provider on open, otherwise falls back to the reflected cook-settings form and the reserved preview
placeholder. Widgets are `UIElement`s so providers live in the shell layer; this is the seam for
asset-specific viewers (mesh/material/terrain) without touching the generic widget.

### D8. Cleanup (single config editor)

The asset browser's details pane is reduced to info + target selector + a "double-click to open the viewer"
hint; the duplicate inline reflected form is removed. Cook settings are edited in exactly one place (the
viewer, or a per-type provider), avoiding two divergent editors.


## Risks / Trade-offs

- **Modal UX**: a modal viewer blocks the editor. → Acceptable v1; a non-modal/dockable variant is a
  follow-up. The viewer is read-mostly.
- **Form input forwarding divergence**: forwarding must match the form's bounds exactly. → Set the form
  bounds in `OnPaint` and forward with the same rect; covered by a shell test.
- **Preview seam drift**: a future renderer preview must implement the same interface. → Keep the interface
  minimal (uuid+type) and document the viewport `ViewContentSource::ASSET` target.

## Migration Plan

Additive: with no provider registered the preview region is a placeholder and cook settings still work.
Rollback removes the widget + wiring; the open handler falls back to logging as today.
