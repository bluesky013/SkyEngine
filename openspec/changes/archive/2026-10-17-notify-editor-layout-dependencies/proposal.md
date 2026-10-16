## Why

Incremental runtime layout is available, but editor forms and hosts still depend on work performed by Paint or forced layout every frame. A reflected form's preferred height currently comes from its previous paint, external property/undo changes are observed too late for host allocation, and world-document/theme changes have no explicit path to invalidate retained editor layout. These dependencies must be notified before selected editor hosts can safely reuse layout.

## What Changes

- Make reflected form row extent and preferred height correct before the first paint. Bind, refresh, expansion, sequence edits, header changes, and relevant registry changes notify layout.
- Correct indexed sequence-element descriptors so scalar rows are not misclassified as their owning container; keep add/remove controls and natural row extent consistent.
- Subscribe forms to their injected property notifier and command service. Record pending refresh/invalidation immediately, then synchronize safely before host measurement/arrangement without replacing active interaction state.
- Add explicit world-document binding/refresh for controlled WorldConfigPanel hosts, while preserving the legacy provider constructor's polling and volatile compatibility.
- Migrate AssetViewer's owned composition to notified incremental layout. Open/reload, child form changes, resizing, and scrolling invalidate the appropriate work; custom provider volatility remains effective.
- Synchronize known editor hosts before shell allocation and use ArrangeIfNeeded there. Unknown hosts retain their conservative policy.
- Add a process-shared default-theme revision and invalidate every element's layout/cache in the affected main/floating surface before its next layout when that revision changes, including changes at the same scale.

## Capabilities

### New Capabilities

- `editor-layout-dependencies`: explicit pre-layout dependency synchronization, controlled document binding, incremental editor-host integration, and conservative extension behavior.

### Modified Capabilities

- `editor-reflected-form`: paint-independent preferred extent and notified structural/external changes.
- `editor-property-model`: distinguish an indexed sequence element from its owning sequence container.
- `editor-asset-viewer`: notified incremental composition with correct first-frame, resize, reload, and scroll allocation.
- `editor-ui-scaling`: shared theme revision and complete surface layout invalidation before reuse.

## Impact

- EditorWidgets reflected forms/registries, EditorUi theme state, and EditorShell's world config, asset viewer, main/floating allocation integration and focused tests.
- docs/editor/ui-layout-and-chrome-plan.md records the notification contract and remaining volatile dependencies.
- Builds on five preceding uncommitted UI changes without changing their artifacts, main specifications, runtime measurement rules, stored document formats, or renderer contracts.
- Runtime UI remains independent of Sandbox. Theme/custom-provider mutation without notification, general reactive bindings, geometry/render caches, localization redesign, and broad host migration remain outside this change.
