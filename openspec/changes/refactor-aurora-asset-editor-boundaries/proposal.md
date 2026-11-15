## Why

Aurora Cook currently links the whole Adaptor to obtain persistent asset data, while Aurora.Metadata registers runtime components and Sandbox WorldEditorState implements Aurora-specific editing policy. These dependencies prevent clean offline cooking and make generic editor behavior depend on renderer details.

The user approved the concrete follow-up architecture on 2026-11-15. This change owns its implementation and validation, separately from the completed asset-reference fixes.

## What Changes

- Add one Aurora.Assets.Static implementation target in the existing adaptor directory, sharing persistent asset data, codecs, pure material definition processing and persistent component schemas.
- Split mixed CPU/runtime files without relocating existing directories; give each production source one compilation owner.
- Make Aurora.Metadata the sole persistent metadata owner and Aurora.Render the sole runtime-component metadata owner; offline tools consume persistent schemas without a renderer session.
- Replace Cook's Adaptor dependency with Assets, preserving existing asset formats, UUIDs and serialized component identities.
- Extend the existing EditorExtension/EditorExtensionHost with explicit domain-edit results, then move Aurora actor creation and skeletal/animation edit policy out of WorldEditorState.
- Preserve transaction failure recovery, undo/redo, document lifetimes, project creation defaults and readable asset labels/tooltips.

## Capabilities

### New Capabilities

None. This change extends existing domain contracts.

### Modified Capabilities

- `assets`: Aurora persistent data and handlers have a reusable implementation owner independent of Adaptor.
- `modules`: Aurora persistent and runtime reflection have separate process-once owners and explicit dependencies.
- `editor-core`: existing extensions prepare domain-owned edits with unambiguous handled/error/conflict results.

## Impact

Aurora adaptor/cook/runtime/editor targets, module manifests, generic editor extension interfaces, WorldEditorState and relevant test initialization. No new plugin, asset manager, renderer hierarchy or source-file relocation. Core remains the existing Aurora foundation dependency; this does not claim complete renderer-free linkage. Commit and archive still require separate user approval.
