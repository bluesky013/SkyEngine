## Why

`AuroraModule` currently combines RendererHost/Launcher behavior with publishing the global animation Skeleton asset source. Creating a renderer provider therefore requires a Skeleton source and a module-manager code lease, even when the renderer host is used directly or another Skeleton provider exists.

## What Changes

- Move Skeleton source construction, registration and teardown to the existing `Aurora.Render` runtime entry's assembly class.
- Keep `AuroraModule` focused on renderer provider, host, window and packet production.
- Preserve the source/request/result module leases and single registration owner.
- Withdraw the Skeleton service before host drain; release the assembly's source after host shutdown. Roll back registration if renderer initialization fails.
- Verify direct host independence, provider-only publication, dynamic/static module loading, repeated sessions and retained request safety.

## Capabilities

### New Capabilities
- `aurora-runtime-service-lifecycle`: Runtime composition of renderer and Skeleton asset service sessions.

### Modified Capabilities

None.

## Impact

The user approved this runtime-entry responsibility change. It stays within the existing Aurora adaptor/runtime directories and targets. Skeleton implementation/data ownership, metadata registration, module names and load dependencies remain unchanged. No new targets or production compile variants are introduced.
