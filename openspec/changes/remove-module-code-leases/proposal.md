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

None.

### Modified Capabilities

- `modules`: Native code residency independent of restartable module sessions and owned data.
  replace distributed code tokens with resident code and explicit object/session ownership.

- `asset-cook`: retain acquired compressor instances without per-provider code tokens.

- `async`: retain accepted callback state and executor owners without caller-supplied code tokens.

- `aurora-renderer`: replace distributed code tokens with resident code and explicit object/session ownership.

- `aurora-shaders`: replace distributed code tokens with resident code and explicit object/session ownership.

- `editor-viewport`: replace distributed code tokens with resident code and explicit object/session ownership.

- `physics`: replace distributed code tokens with resident code and explicit object/session ownership.

- `platform`: drain asset work before destroying its services and resources while module code remains resident.

- `navigation`: retain navigation meshes, generators and query data while provider code remains resident.

## Impact

Existing Core/Framework/Aurora/Content/Animation/Editor/Physics targets and source owners only. This is separate from the completed Vulkan configuration commit. The review also guards callback dispatch and registration reentry, synchronizes active lifecycle contracts, and preserves historical verification evidence. No directory relocation, new target, binary hot reload or OpenSpec archive is introduced. The external PartySoftBody compatibility edits are outside the engine Git repository.
