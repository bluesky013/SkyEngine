## Why

Closing a floating editor window destroys its UI renderer while the shared font atlas still retains that renderer's texture-registry pointer. Requesting a new glyph or atlas page later invokes a freed registry. The review reproduced this with AddressSanitizer.

## What Changes

- Add paired, idempotent removal for the atlas/text system's borrowed texture registries.
- Detach floating renderers on explicit close, OS-close, and global shutdown before destroying them.
- Clear the text system's default registry when that registry is removed and allow explicit reattachment.
- Add regression coverage for duplicate registration/removal, replay after reattachment, and new glyph/page requests after destroying a detached registry.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `ui-text`: explicit lifetime contract for borrowed atlas registries and deterministic reattachment.

## Impact

- UI runtime text headers/implementation, UITextTest, and EditorRender surface cleanup.
- No backend dependency in UI core or persisted format change.
- GPU resource retirement and cross-surface icon/color-wheel replay remain separate tasks T02/T07.
