## Why

The user established a process-resident DLL contract: plugin updates, replacement and removal take effect after restarting the process. Distributed code leases add lifecycle parameters and registration prerequisites for an in-process code-unload capability that is not required.

## What Changes

- Keep successfully loaded DynamicModule libraries resident in a cross-DLL process owner; releasing a wrapper does not physically unload native code.
- Separate initialized module sessions from resident libraries and preserve process-once metadata definitions.
- Resolve shared Singleton instances through Environment instead of caching pointers in resident DLLs.
- Remove RetainModule and pure code-lease fields/parameters from metadata, assets, World registration, animation sources, render features, editor providers, shader compiler handles and Jolt bridges.
- Preserve real compiler, SDK, asset, request, World, Feature, document, provider and GPU ownership and all drain/retirement protocols.
- Update tests, repository rules and the World/shader guides to express process residency, initialized dependencies and actual object lifetimes.
- Adapt the two PartySoftBody Feature registrations affected by the public API change.

## Capabilities

### New Capabilities
- `process-resident-module-code`: Native code residency independent of restartable module sessions and owned data.

### Modified Capabilities

- `asset-cook-config`: retain acquired compressor instances without per-provider code tokens.
- `async-threadpool`: retain accepted callback state and executor owners without caller-supplied code tokens.
- `aurora-adaptor`: replace distributed code tokens with resident code and explicit object/session ownership.
- `aurora-renderer`: replace distributed code tokens with resident code and explicit object/session ownership.
- `aurora-shader-cache`: replace distributed code tokens with resident code and explicit object/session ownership.
- `aurora-shader`: replace distributed code tokens with resident code and explicit object/session ownership.
- `editor-viewport-picking`: replace distributed code tokens with resident code and explicit object/session ownership.
- `jolt-integration`: replace distributed code tokens with resident code and explicit object/session ownership.
- `module-registration`: replace distributed code tokens with resident code and explicit object/session ownership.
- `native-platform-services`: drain asset work before destroying its services and resources while module code remains resident.
- `navigation-runtime`: retain navigation meshes, generators and query data while provider code remains resident.
- `physics-backend`: replace distributed code tokens with resident code and explicit object/session ownership.
- `physics-serialization`: replace distributed code tokens with resident code and explicit object/session ownership.

## Impact

Existing Core/Framework/Aurora/Content/Animation/Editor/Physics targets and source owners only. This is separate from the completed Vulkan configuration commit. The review also guards callback dispatch and registration reentry, synchronizes active lifecycle contracts, and preserves historical verification evidence. No directory relocation, new target, binary hot reload or OpenSpec archive is introduced. The external PartySoftBody compatibility edits are outside the engine Git repository.
