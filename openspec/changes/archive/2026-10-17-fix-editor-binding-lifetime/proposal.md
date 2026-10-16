## Why

The UI review found a model lifetime regression in reflected forms. Bind and destruction cancel a color preview through an old PropertyField descriptor even when WorldConfigPanel has already replaced config storage or AssetViewer's model members have been destroyed. Clearing an interaction must distinguish restoring a live model from abandoning an invalid binding.

## What Changes

- Define explicit cancel and abandon interaction resets. Bind, Unbind, and form destruction abandon callbacks without accessing the former model; CancelInteractions and valid-model Refresh restore previews.
- Expire a per-binding callback token whenever fields/interactions are reset, independently of notifier subscription lifetime. Built-in color and inline-edit callbacks reject an expired binding.
- Cancel and detach AssetViewer forms before replacing settings; clear child forms before owned model members are destroyed. Closing/reopening the viewer cancels live previews.
- Abandon WorldConfig bindings before resolving config after same-address reload. Cancel a valid config preview before switching the selected subsystem and saving it.
- Migrate Preferences page-switch/close/snapshot restoration to explicitly cancel while its page model remains valid.
- Cover released-model rebind/unbind/destruction, ordinary cancellation, real WorldConfig reload, and heap-backed AssetViewer settings reload/close/destruction.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `editor-reflected-form`: explicit binding invalidation and distinct live cancellation/abandonment.
- `editor-asset-viewer`: child/model destruction ordering and safe settings replacement.

## Impact

- EditorWidgets, WorldConfigPanel, AssetViewerWidget, PreferencesDialog, and headless editor regression suites.
- Bind no longer implicitly rolls back the previous model. A host requiring rollback must call CancelInteractions while that model is valid, before replacement. Custom widgets must accept ReflectedInteractionReset and must never access model data during Abandon; callbacks retained outside their widget should check BindingLifetime.
- Refresh still requires a live bound model. Hosts that replaced storage must use Unbind/Bind or WorldConfigPanel::RefreshDocument before further synchronization or input.
- This change does not repair raw command-history targets or asset undo persistence (T04), or shell-wide pre-history cancellation (T05). No commit or OpenSpec archive is included.
