## Why

RemoveChild and ClearChildren currently change ownership without notifying UIEventRouter. A router
can retain focus, hover and per-pointer capture into a detached or destroyed subtree; hosts compensate
with whole-router Reset calls. Event bubbling and hover transitions also read raw targets after user
callbacks, even when those callbacks remove the same branch. Native containers additionally cache
direct-child capture outside the router.

## What Changes

- Associate live tree attachments and event routers with their owning UIContext, and invalidate an
  attachment whenever it leaves the tree. A later reattach starts a fresh interaction lifetime.
- Remove only focus, hover, router capture and direct-child capture affected by a subtree detach,
  before invoking cancellation code. Preserve unrelated sibling state and all state during MoveChild.
- Cancel actual targets without inventing pointer-leave coordinates; deduplicate target/pointer pairs.
- Guard router bubbling, hover transitions and native direct-child routing against callback-driven
  removal, destruction, reattachment and router/context teardown.
- Keep Reset silent and reject focus targets belonging to no context or a different context.
- Add portable CPU and ASan regressions, including retained Button removal/reattachment and destructive
  pointer, key, text, hover and cancellation callbacks.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `ui-input`: attachment-scoped input state, precise detach cancellation and callback-safe dispatch.

## Impact

engine/ui/core tree, context, input router, direct-child routing helpers and native containers, plus
UICoreTest. Existing ownership remains unique_ptr based. No renderer, RDG, RHI, Core Event,
SandboxModule, native window or GPU resource contract is changed. Windows verification stays in the
recorded follow-up queue. This change does not include commit or archive.
