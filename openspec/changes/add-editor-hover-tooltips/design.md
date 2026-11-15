## Context

The editor already has retained UIElement trees, attachment-aware input references, reflected field metadata, catalog snapshots, shared toolbar actions and a common theme/text system. Tooltip display is currently local to the toolbar. Several editor surfaces manually route input: the main shell sends events directly to its active modal; floating asset tools bypass their surface router; Preferences and AssetViewer route their own popups and forms. A tooltip implementation based only on the main router's hover state would miss those paths or reveal content behind an active overlay.

This change owns the shared tooltip seam and the approved first providers. Existing canonical domains are `ui`, `editor-shell`, `editor-inspector`, `editor-assets` and `platform`. Existing module owners, target boundaries and source paths stay intact.

## Goals / Non-Goals

**Goals:**

- Show stable, readable help after 500 ms over one target on one surface.
- Reuse one lifecycle, placement and drawing policy for toolbar, reflected fields, assets and picker entries.
- Preserve modal/popup priority, scroll clipping, transformed geometry and per-window DPI.
- Discard stale targets after rebind, rebuild, close, detach, focus loss or surface retirement.
- Build text only when it is due to appear, using already available metadata and catalog data.

**Non-Goals:**

- Interactive tooltip widgets, tooltip asset loading, previews, navigation or action invocation.
- A global tooltip registry/manager, native popup windows, new targets or a UI event-router rewrite.
- Replacing existing reflected editing, popup capture, asset resolution or catalog refresh contracts.
- Adding new per-configuration handwritten settings forms or migrating unrelated legacy controls.

## Decisions

### Optional generic query and deferred content

UIElement exposes an optional `QueryTooltip`/`BuildTooltip` protocol using UI-domain data only. Query receives untransformed absolute layout-surface coordinates, matching existing element event handlers. A result identifies a stable semantic target within the provider, its anchor rectangle and a weak binding token. The host also retains the provider's attachment-aware input reference; no owning callback captures a form model or asset record.

Query is a cheap hit/identity lookup. Build constructs owned display text only after the delay and after revalidating the provider, binding and current query. A virtualized row uses asset/field/action identity and binding generation, not its current list index or the address of a recycled row. A provider with no content yields no tooltip. This avoids constructing strings on every mouse move and avoids a second editor-specific hierarchy below UIElement.

The host maps a surface point through the candidate element's geometry before querying. It maps the anchor back for overlay placement and applies the visible clip chain. Non-invertible, hidden or clipped targets do not participate. Ordinary traversal reuses UIEventRouter hit testing, then queries the hit element and its ancestors; a foreground element without tooltip content still blocks lower siblings. Manually routed controls expose their effective tooltip branch explicitly.

### One EditorWidgets host per surface

TooltipHost owns only presentation state: candidate identity/reference/token, pointer, elapsed time, resolved content and placement. EditorShell owns one host for the main surface and one for each floating surface. Shell supplies the current effective input root, surface coordinates, monotonic elapsed time and available bounds. A stationary pointer can reach 500 ms without requiring another pointer event.

Changing target, provider attachment or binding restarts the delay. Pointer leave/cancel, pointer press, scroll, key/text input, focus reset, modal/root replacement and surface retirement clear the pending or visible tooltip. Actual drag/pressed interactions suppress display. Open picker/dropdown capture is not by itself a drag: its popup remains eligible while controls behind it are excluded. No tooltip participates in hit testing, focus, capture or command routing.

Provider text is copied when built; later lifetime checks never dereference a stale field. Tick discovers targets and advances hover timing. Paint only validates the retained root, owner, geometry and binding; it does not traverse the tree again. The host is destroyed with its surface; moving a panel between surfaces cannot transfer its old hover timer.

### Native hover-leave notification

The existing IMouseEvent interface gains a default-empty `OnMouseLeave(WindowID)` callback. Cocoa adds entered/exited events to its existing visible-view tracking area and forwards `mouseExited:` only while its C++ owner is alive. Win32 registers `TME_LEAVE` on ordinary mouse motion and forwards `WM_MOUSELEAVE`; a registration failure is logged and later motion can retry. Captured motion does not re-arm tracking outside the client area. X11 subscribes to `LeaveWindowMask` and forwards normal `LeaveNotify` crossings, excluding grab/ungrab transitions and movement into a child window.

This event identifies the source window; it carries no fabricated coordinates, button release or capture cancellation. SandboxModule resolves that window's editor surface and cancels only its tooltip. Native drag/capture routing and pressed-button state remain intact. No global cursor polling or new native window manager is introduced. Cocoa native-entry regression tests verify the window identity, tracking option, subsequent input delivery and retained-view teardown; Windows/Linux execution remains separately reported when unavailable on the host.

### Preserve actual routing ownership

EditorShell selects roots consistently with input dispatch. Main-window modal priority remains New World, Preferences, file browser, then the embedded asset viewer. Ordinary floating panels are blocked by an active main modal, while independent floating asset tools keep their established independent routing. AssetViewer's close-decision prompt and menu/popups take precedence over its toolbar and content. Preferences queries only its active page/form or popup, respecting the category and form viewports.

Relevant existing integration points:

- `EditorShell::DispatchPointer` and `DispatchPointerToSurface`: early modal/asset-tool returns must still feed or clear the correct host.
- `EditorShell::PrepareSurfaceInput` and `PaintSurface`: synchronize layout and the surface theme before querying or positioning.
- `EditorShell::Paint`: paints the main tooltip last; `PaintSurface` does the same for floating surfaces after UI content.
- `EditorShell::ResetInput`, `TickAssetViews` and panel transfer/rebuild: clear stale surface state before contexts or attachments disappear.
- `AssetViewerWidget::OnPointerEvent`, `NeedsCloseDecision` and `SynchronizeLayout`: preserve close prompt/menu/content priority and invalidate bindings when content reloads.
- `PreferencesDialog::OnPointerEvent`, `BindPage` and `SynchronizePages`: account for manually routed form popup capture and page replacement.
- `ModalDialog::RouteContentPointer`: popup routing can extend outside a child's normal bounds; tooltip queries must not fall through to covered content.

This is local shell integration, not a new event dispatch system. Weak lifetime checks must precede any member access after callbacks that can close/rebuild the shell or provider.

### Shared text and placement

The overlay uses UiTheme/UiSkin colors and metrics and UITextLayout measurement/wrapping. The host prefers an anchor-adjacent position, flips when needed and clamps to the available surface bounds. Long paths wrap, including unbroken runs; narrow windows and explicit line breaks remain bounded. Coordinates are converted once, and final painting occurs outside child scroll clips under the correct surface scale. Tooltip painting does not mutate layout or retained input state.

### First providers

- Reflected forms use existing TOOLTIP metadata for the hovered field/label. Missing or empty metadata yields no invented description. Binding/refresh invalidates the former target token.
- Asset picker entries and asset references expose full logical path, type, available catalog status and GUID from the current snapshot. Missing/unresolved identities are reported accurately without resolving or loading products. Extra AssetBrowserPanel tooltips are outside this first provider set.
- Picker entries expose the same identity for the hovered visible result, independent of selection. Filtering, list revision, scroll clipping and close invalidate the previous candidate.
- ToolBar supplies its action label through the same protocol. Its local tooltip rendering is removed so a label is painted once by the surface host.

## Risks / Trade-offs

- Manual routing differs from tree hit testing -> select the active branch explicitly and cover modal, popup, Preferences and standalone AssetViewer regressions.
- A row survives while its backing model changes -> require a binding token/generation as well as element attachment lifetime and requery before build/paint.
- Popup capture can be mistaken for dragging -> suppress real pressed/drag state without suppressing eligible picker entries.
- Mixed transforms/DPI or retained scroll clips can place the overlay incorrectly -> query in element layout coordinates, transform the anchor and paint last in surface space with its theme.
- Catalog data may change during the delay -> read the current snapshot when building text, without starting an asset request. Visible text remains a snapshot until the hover target or binding is invalidated.
- Font metrics and long identifiers vary -> use UITextLayout and bounded wrapping, with narrow-window and multiple-scale checks.

## Migration Plan

1. Add the optional UI protocol and shared EditorWidgets host with focused unit coverage.
2. Implement providers using existing metadata, catalog snapshots and action labels.
3. Wire main/floating shell hosting and manual modal routes, then remove the toolbar-only painter.
4. Build affected production editor/UI targets and run protocol/host/provider/shell suites; record actual counts and any skipped visual/platform checks.
5. Perform a bounded editor smoke/visual check for delayed display, reflected fields, asset details, picker entries and floating surfaces. Keep this change active until evidence is recorded; commit/archive require their existing user confirmation.

## Open Questions

None blocking the approved implementation. Exact private helper names can follow the existing code; the ownership, coordinate, timing and side-effect contracts above remain fixed.
