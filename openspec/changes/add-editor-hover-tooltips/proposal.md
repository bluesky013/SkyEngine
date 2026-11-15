> Status: active; implementation authorized. This change owns the common editor hover-tooltip protocol and first providers. Validation remains pending until recorded in tasks and verification; no commit or archive is included.

## Why

Editor controls do not consistently expose descriptions or complete asset identities on hover, and the toolbar's local tooltip cannot provide shared timing, clipping, placement or lifetime handling. A common surface-owned host makes reflected help and asset details available without duplicating popup behavior across widgets.

## What Changes

- Add an optional UIElement tooltip query/build protocol with layout-coordinate hit queries, stable target identity, anchor geometry and non-owning binding lifetime.
- Add one TooltipHost per editor surface, with a 500 ms hover delay, cancellation on input/lifetime changes, bounded placement and final overlay painting.
- Route discovery through the currently effective input root, including modal dialogs, Preferences, floating panels and independent asset viewers.
- Forward native desktop mouse-leave notifications with the originating WindowID so a focused window can dismiss hover content without canceling a captured gesture.
- Provide reflected TOOLTIP metadata, asset full logical path/type/status/GUID, picker entry details and toolbar labels through the same host.
- Reuse UiTheme/UiSkin and UITextLayout; tooltip display remains non-interactive and never loads an asset or invokes an action.
- Add focused protocol, timing, invalidation, provider, modal/floating-surface and text-layout regression coverage, plus a bounded editor visual check.

## Capabilities

### New Capabilities

None; the change extends existing canonical domains.

### Modified Capabilities

- `ui`: optional lifetime-safe tooltip target discovery and deferred content construction on UIElement.
- `editor-shell`: per-surface delayed tooltip hosting, active-root routing, themed final-overlay layout and toolbar integration.
- `editor-inspector`: reflected TOOLTIP content and invalidation when form bindings change.
- `editor-assets`: catalog-only asset and picker tooltip details with complete logical identity.
- `platform`: native desktop hover-leave notification through the existing optional mouse-event interface.

## Impact

Existing Framework input/native platform, UI, EditorWidgets, EditorShell and provider/test files only. The generic UI protocol does not depend on editor types; editor presentation remains in the existing editor targets. No module/target ownership changes, directory moves, global tooltip manager, native popup windows or new asset-loading paths are required. The protocol is optional for existing UIElement subclasses. Existing editor input capture, modal priority and reflected editing contracts remain authoritative.
