## Context and baseline

`refactor-aurora-shared-pass-headers` supplies shared `.h` declarations and removes Slang from C++ type generation. Its built-in Scene bundle is embedded through `SCENE_SHADER_PATHS` and `RegisterSceneShaderSources`; those are concrete pipeline integration, not the ownership contract for every plugin.

Current limitations are specific: `AURORA_SHADER_PASS` fixes `sky::aurora::shared`; virtual scene paths such as `ScenePass.h` are unqualified; `AddVirtualFile` replaces an existing path; native search roots use first-match priority; the cook CLI accepts a source directory rather than an owner/dependency description; generic `PassResourceBindings` is supplied by `Aurora.Pipeline.Static`. Existing `RegisterRenderFeature`, `IFrameGraphFeature`, `moduleLease`, `ShaderFileSystem`, `MultiFileSystem` and cache fingerprints are the reuse points.

## Ownership layers

| Layer | Owns | Depends on |
|---|---|---|
| Aurora foundation | Declaration helpers, compiler/resolver interfaces, generic binding validation, FrameGraph and backend execution | Existing Core/RHI dependencies; never user plugins or concrete pipeline declarations |
| Built-in pipeline | Scene/Shadow/ToneMap implementation, Global/Pass/Batch declarations, shader bundle and assembly choices | Aurora foundation |
| User pipeline plugin | Its Pass implementations, shared headers, shaders, bundle description and assembly choices | Aurora foundation plus explicitly declared interfaces/resource dependencies |
| Host/adaptor | Selects and assembles enabled pipeline features; connects project/plugin resources and actual module leases | Consumer-facing interfaces and the selected implementations |

Global/Pass/Batch are resource tiers (sets 0/1/2), not module or directory layers. Each pipeline defines the layouts it uses; View remains optional. A custom pipeline does not inherit built-in GlobalBlock/ScenePass/SceneBatch layouts, static/dynamic policy, or CompileSceneMaterial behavior. Cross-owner layout reuse is explicit.

## Source and build layout

Illustrative external project layout, not a new engine target hierarchy:

```text
<UserProject>/plugins/my_pipeline/adaptor/
  CMakeLists.txt
  src/MyPipeline.cpp
  src/ToonPass.cpp
  shaders/my_pipeline/ToonPass.h
  shaders/my_pipeline/ToonPass.slang
```

The plugin target includes its shared headers directly through its private include roots; shared headers include Aurora's public declaration helper. The same source is included by Slang. Extend the existing macro with an explicit C++ namespace choice; exact macro spelling is an implementation detail. Two plugins may both define ToonPass/ToonParams in different namespaces and compile into the same consumer without an ODR conflict. The built-in namespace remains a deliberate choice of its own declarations.

Each owner registers its own resource inputs using a reusable CMake helper integrated with the existing resource deployment/package workflow. The helper produces data describing stable owner ID, logical root, source-to-logical mappings and declared owner dependencies. It does not run Slang to produce C++ types. Packaging may copy source files, store them in an existing package filesystem or embed a text table, but all representations preserve the same logical identity and source contents. No generated shader mirror is required, and no outputs are written back into source trees.

An owner-local build staging directory may mirror its logical shader paths before deployment; user resources remain in the user's plugin artifact. A shared logical namespace does not imply a single physical directory or one central CMake source list.

## Logical paths and resolution

Proposed logical examples:

```text
aurora/shader/SharedShader.h             # foundation helper
aurora/pipeline/ScenePass.h              # built-in owner
plugins/my_pipeline/ToonPass.h           # external owner
plugins/my_pipeline/ToonPass.slang
```

Bundle registration validates canonical relative paths and owner identity before publishing the bundle. Different owners can reuse local basenames; duplicate fully qualified paths or conflicting owner registrations fail explicitly and leave the existing set unchanged. Absolute paths and traversal outside an allowed root are invalid. Do not use filesystem search order or AddVirtualFile replacement to select a pipeline or override another owner.

Quoted includes first resolve relative to the including logical file. Qualified includes resolve against the owner's bundle and explicitly declared dependencies, including the common Aurora helper. Missing/undeclared dependency errors identify the requested logical path. Preserve current supported include semantics and source-closure tracking; extend the existing filesystem resolution seam rather than building another independent VFS. Review both Slang include loading and ShaderResolver hashing so they resolve the same dependency contents.

## Runtime, cook and cache

Runtime and ShaderCookTool consume the same build-produced bundle description and dependency graph. Cook reads resource metadata and source packages without loading the runtime pipeline DLL or the editor. Native directories, packages and embedded sources produce the same logical file identities and contents. Header edits participate in both build dependencies and source-closure fingerprints.

Reuse existing variant/target/layout/toolchain/source cache keys. Qualified paths and dependency contents must prevent two owners with the same basename from aliasing in the cache. Migration of built-in unqualified paths is coordinated with ShaderRef/companion/usage references and cache invalidation; do not leave silent global aliases as a second permanent lookup path. A new binary cache format is not assumed unless implementation evidence requires it.

## Binding helper and pipeline assembly

Propose relocating only generic PassResourceBindings into the existing Aurora Core renderer API/implementation, keeping its transactional validation and frame-borrowing contract. Candidate paths are `core/include/aurora/renderer/PassResourceBindings.h` and `core/src/renderer/graph/PassResourceBindings.cpp`; the existing Aurora.Core.Static and interface include roots suffice. Concrete Scene/Shadow/ToneMap declarations remain in Pipeline. Review this narrow ownership/include migration before implementation; do not create new targets or move unrelated directories.

The host explicitly selects which pipeline features to register in its existing first packet. Built-in scene registration is one selection, not a prerequisite or an implicit fallback for every host. A user pipeline registers its IFrameGraphFeature implementations using existing IDs, feature commands and real module leases. Keep one Renderer/Device and one host packet producer; do not create per-plugin sequences, queues or event consumers. Reuse existing graph dependencies for composition, rather than infer pass order from resource search paths.

## Lifetime and retirement

Compilation captures a stable source/dependency view with ownership sufficient to finish or explicitly cancel the job. Registration changes affect future requests; they must not mutate or destroy a filesystem borrowed by an in-flight ShaderCompileTask. A retained filesystem/source snapshot owns the necessary backing data; source-provider code retains its defining module when required.

Retirement stops new feature/source work, collects compile tasks, and lets existing frame/GPU/presentation borrowers drain through the established protocol. Existing moduleLease and resource leases protect feature/provider code until the last use. Compile failures and discarded frames release their borrows through the same path. This change supplies lifecycle correctness at the existing seams, not a new automatic hot-reload scheduler.

## Alternatives and decisions

- One global engine shader directory/list: rejected because it makes the engine enumerate external plugins and obscures ownership.
- Require custom pipelines to reuse built-in SceneMaterial and layouts: rejected because those are concrete pipeline policies.
- Generate `.slang` mirrors into source trees: rejected because shared headers already serve both languages and generated source would introduce another persistence contract.
- Depend only on first-match search paths: rejected because two plugins can reuse names and cook/runtime mount order can differ.
- New registries/compilers/resource systems: avoid; extend the existing CMake, filesystem, resolver and feature seams with owner metadata.

## Implementation order and validation

1. Review exact module/include changes and host selection seam; inventory existing source/package/deployment helpers.
2. Enable owner-scoped declarations and generic binding reuse, then establish bundle/path resolution contracts.
3. Connect the same bundle input to runtime and cook; migrate built-in references and retire old unqualified registration assumptions.
4. Add external-style fixtures and lifecycle coverage; run existing built-in regressions and document migration.

Acceptance requires two plugin fixtures with the same local Pass/field/file names, one pipeline without a View block, and an external pipeline with no built-in Pipeline library dependency. Test duplicate registration rollback, undeclared/missing includes, shared dependency edits, resource representation equivalence, source removal while compilation is in flight, and retirement with queued/GPU work. Compile source/reflection for supported SPIR-V/DXIL/MSL targets; native Metal execution remains a separately reported macOS gate.

## Stage boundary

This change is proposal-only at the current shared-header checkpoint. Do not claim plugin packaging, namespaced mounts, generic helper relocation or custom host selection are already implemented. Main specifications are updated when this change is implemented and explicitly archived, not by drafting these deltas. Archive/commit require the repository's separate user confirmation.
