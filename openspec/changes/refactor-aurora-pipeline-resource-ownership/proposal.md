## Why

The shared-header refactor removed the C++ dependency on shader compilation, but its integration currently serves Aurora's built-in pipeline. The shared macro fixes the C++ namespace, scene sources use unqualified virtual paths, and generic PassResourceBindings lives beside concrete pipeline implementations. Extending this arrangement directly to user plugins would mix ownership, allow name collisions, and require runtime and cook to maintain different resource lists.

User pipelines must own their Pass implementations, shared headers and shader sources. Aurora supplies the declaration, compilation, binding and frame execution mechanisms; the built-in pipeline is another consumer of those mechanisms.

## What Changes

- Let shared declarations select an owner-specific C++ namespace while retaining one `.h` source for C++ and shaders.
- Describe shader bundles per owner/target, with qualified logical paths and explicit include dependencies. Reuse the existing filesystem, packaging and shader cache mechanisms.
- Have runtime compilation and offline cook consume the same bundle description, independently of physical source/install/package locations.
- Expose generic Pass binding functionality through existing Aurora foundation targets, without requiring a user pipeline to link the built-in pipeline implementation.
- Assemble selected pipelines through existing feature commands and module leases; retain source snapshots until compile jobs and dependent rendering work retire.
- Migrate the built-in scene bundle as one owner and validate two independent external-style pipeline fixtures.

## Capabilities

### Modified Capabilities

- `aurora-shader`: owner-scoped declarations, qualified shader resources, shared runtime/cook inputs and compile snapshot lifetime.
- `aurora-renderer`: foundation binding access and explicit built-in/user pipeline assembly through existing feature lifecycle.

## Impact

SharedShader helpers, ShaderFileSystem/ShaderResolver integration, existing CMake resource helpers, ShaderCookTool, Pipeline PassResourceBindings, built-in scene registration and host assembly. A narrowly scoped helper relocation into existing Aurora Core is proposed; no new Aurora plugin, renderer target hierarchy, compiler, cache or submission system is proposed.

## Non-goals

- Do not implement this proposal as part of `refactor-aurora-shared-pass-headers` closeout.
- Do not move user plugin code/assets into engine directories or collect every owner's source into a central list.
- Do not generate a second editable `.slang` ABI mirror, make C++ depend on Slang, or alter Global/Pass/Batch set ownership.
- Do not introduce a general plugin hot-reload system, editor pipeline-selection UI, new storage declaration capabilities or unrelated directory moves.

## Status

2026-10-21: proposal/design/spec/task drafting only; implementation has not started. Depends on the implemented shared-header work. Review the proposed helper ownership and host assembly changes before implementation, as required by repository module-boundary rules. Current-stage archive and commit are separate actions, not performed by creating this change.
