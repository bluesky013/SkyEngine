## Why

`AuroraModule` previously combined RendererHost/Launcher behavior with publishing the global animation Skeleton asset source. Creating a renderer provider therefore required a Skeleton source and a module-manager code token, even when the renderer host is used directly or another Skeleton provider exists.

## What Changes

- Move Skeleton source construction, registration and teardown to the existing `Aurora.Render` runtime entry's assembly class.
- Keep `AuroraModule` focused on renderer provider, host, window and packet production.
- Preserve actual source/request/result ownership and the single registration owner; the subsequent loader cleanup provides process-resident code.
- Withdraw the Skeleton service before host drain; release the assembly's source after host shutdown. Roll back registration if renderer initialization fails.
- Verify direct host independence, provider-only publication, dynamic/static module loading, repeated sessions and retained request safety.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-renderer`: Runtime composition of renderer and Skeleton asset service sessions.

## Impact

The user approved this runtime-entry responsibility change. It stays within the existing Aurora adaptor/runtime directories and targets. Skeleton implementation/data ownership, metadata registration, module names and load dependencies remain unchanged. No new targets or production compile variants are introduced.
