## Why

AssetViewer persisted edits in an OnEdited callback and immediately replaced its settings Any. Generic property history still retained pointers into that released model, while Undo only changed the old in-memory object and never restored persisted cook settings. Viewer chrome also traversed global editor history instead of a history owned by the asset editing session.

## What Changes

- Give AssetViewer an injected local CommandService shared by its reflected forms. Opening/reopening/switching an asset starts a fresh session; close clears that history. Borrowed catalog services must outlive the session.
- Add a UI-free ReflectedForm command factory and route scalar/reset/sequence edits through it. Existing forms retain generic descriptor commands by default.
- Add catalog-domain commands identified by asset UUID, target, reflected schema UUID, and before/after target sparse overrides. Commands retain no viewer, field, TypeNode, or settings storage pointer.
- Execute Do/Undo/Redo through one target-specific persistence and observer-notification operation; preserve every other target and unrelated cook-block data.
- Resolve current asset/target/schema at execution. Missing/read-only assets, removed targets, or replaced reflected schemas make stale commands inert. The viewer clears history when it observes invalid targets/models/schema; persistence failures never fall back to stale model writes.
- Route viewer scoped toolbar/menu/accelerator actions to its local session and reload materialized settings after command changes, including external traversal of the injected service.
- Cover heap-backed settings release, persistence, target/asset isolation, reset/sequence factory routing, missing identity/schema, close/reopen/switch, and shell actions.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `editor-reflected-form`: optional domain command construction for all committed form mutations.
- `editor-asset-viewer`: owned edit-session history and stable persisted cook-setting commands.

## Impact

- EditorCore ReflectedForm/EditorAssetCatalog, EditorShell AssetViewer/actions, and core/shell regressions. No asset business is added to CommandService.
- Depends on T03's explicit live cancellation/detachment and integrates with T05's injected before-history notification.
- Generic DATA view editing remains tied to its session-owned model; arbitrary provider-specific source writeback and external provider-owned UI histories remain separate work.
- No commit or archive is included.
