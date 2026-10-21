## ADDED Requirements

### Requirement: Owner-scoped shared declarations

Shared declarations SHALL use one authoritative C++ `.h` source with owner-selectable namespaces. C++ consumers SHALL include that source without requiring Slang compilation or shader-header export. CMake tooling SHALL provide deterministic export/update and read-only verification of shader-only `.slangh` assets. Exports SHALL persist in the module's source-tree `assets/shaders/` root, be tracked in version control, and preserve the declaration's domain-relative path with only the extension changed. Generated headers SHALL NOT be independently editable sources or complete shader programs. Global/Pass/Batch SHALL remain sets 0/1/2, with optional View data inside Pass.

#### Scenario: Two plugins reuse local names
- **WHEN** independently built plugins declare ToonPass and ToonParams in separate namespaces
- **THEN** their C++ declarations coexist without symbol collisions and their respective shaders consume the corresponding exported interfaces

#### Scenario: C++ build without shader compiler
- **WHEN** a C++ target consumes an owner's shared declaration with Slang unavailable
- **THEN** its types, slots and expected layout metadata compile directly from the authoritative header without compiling shaders or executing a target-platform generator

#### Scenario: Deterministic shader header export
- **WHEN** the same declaration, helper and exporter inputs are exported repeatedly
- **THEN** they produce identical persisted headers without changing files whose contents are already current, and updated inputs require corresponding export and shader-cache identity updates

#### Scenario: Fresh checkout or build clean
- **WHEN** shader assets are read from a fresh checkout or CMake build outputs are cleaned
- **THEN** the tracked `.slangh` assets remain available under the module's assets/shaders root without relying on a generated build-directory copy

#### Scenario: Stale or manually modified export
- **WHEN** source-checkout shader build/cook or CI finds a missing export or one inconsistent with its registered declaration/helper/exporter inputs
- **THEN** verification reports the source/output mismatch without silently modifying tracked assets or shipping inconsistent interfaces

#### Scenario: Configuration-independent export
- **WHEN** different build configurations or shader backends consume the same shared declaration
- **THEN** they consume the same persisted interface without racing to overwrite it with configuration-specific output

#### Scenario: Export collides with handwritten shader helper
- **WHEN** an export mapping claims a path already owned by a handwritten `.slangh` file
- **THEN** registration/export fails before overwriting the existing asset

#### Scenario: Unsupported declaration syntax
- **WHEN** a shared header uses a construct outside the supported export grammar
- **THEN** export fails explicitly with source diagnostics rather than emitting an incomplete or guessed shader layout

#### Scenario: Shader consumes the exported interface
- **WHEN** a material shader targets OpaquePass
- **THEN** it can include the exported Global and OpaquePass shader headers without including C++ headers or acquiring a generated complete material program

### Requirement: Unified module resource registration

Plugin/module registration SHALL declare shader import rules once in a resource description available to project assembly and offline tools without loading rendering runtime DLLs. Common owner, source-root, namespace, inventory, dependency and distribution metadata SHALL be shared with asset-resource registration where applicable. Shader include/header exports and path identities SHALL retain their own semantics; sharing registration SHALL NOT require converting ShaderRef into a UUID asset. Aurora foundation and project modules SHALL use the same contribution contract without becoming business plugins.

#### Scenario: One declaration feeds all consumers
- **WHEN** an enabled plugin contributes shader resources and asset resources through its module metadata
- **THEN** project collection supplies the relevant rules to development loading, compilation, cook and packaging without separate manually synchronized shader lists

#### Scenario: Runtime DLL is unavailable during cook
- **WHEN** project cook collects declared plugin shader inputs on a host without that plugin's rendering runtime DLL
- **THEN** it obtains resource descriptions and offline-accessible source providers without instantiating runtime rendering services

#### Scenario: Custom filesystem without enumeration
- **WHEN** a module supplies a custom IFileSystem provider
- **THEN** registration requires an explicit immutable file inventory and retained provider ownership, and cook uses an equivalent offline-accessible provider or staged input

#### Scenario: Project selects module contributions
- **WHEN** a plugin is compiled but not selected by the project's resource graph
- **THEN** its roots are not implicitly mounted merely because the plugin exists on disk

### Requirement: Uniform module shader roots and unique paths

Shader-bearing module owners SHALL collect `.slang`, `.slangh` and companion assets under their `assets/shaders/` roots. Aurora SHALL use `engine/aurora/assets/shaders/` across its internal targets; each plugin SHALL use its own `plugins/<plugin-id>/assets/shaders/` root. ShaderRef, includes/imports, dependency hashing and packaging SHALL use canonical domain-relative paths inside those roots, independently of physical source/package locations. Exported headers SHALL reside alongside handwritten assets and preserve the source declaration's domain-relative path and spelling. Existing MultiFileSystem SHALL compose configured providers only after registration validates complete inventories, allowed dependency closure and unambiguous include mappings. Conflicts SHALL fail atomically; mount/include order SHALL NOT resolve them.

#### Scenario: Aurora shared declaration export
- **WHEN** the registered shared declaration has logical include path `aurora/pipeline/OpaquePass.h`
- **THEN** its export resides at `engine/aurora/assets/shaders/aurora/pipeline/OpaquePass.slangh` and shader includes use `aurora/pipeline/OpaquePass.slangh`

#### Scenario: Independent plugin asset roots
- **WHEN** plugin toon exports `toon/pipeline/ToonPass.h`
- **THEN** the persisted file is `plugins/toon/assets/shaders/toon/pipeline/ToonPass.slangh`, its shader include is `toon/pipeline/ToonPass.slangh`, and it needs no repository-wide plugins/assets/shaders root

#### Scenario: Same basename in separate owners
- **WHEN** two plugins export local ToonPass.slangh headers under different owner prefixes
- **THEN** qualified includes resolve to the intended header regardless of registration order or physical package location

#### Scenario: Conflicting registration is atomic
- **WHEN** a contribution conflicts with an existing owner, canonical file path or generated output
- **THEN** registration fails with both providers identified and previously published resources remain unchanged

#### Scenario: Case-only collision
- **WHEN** two contributions expose paths differing only in case
- **THEN** collection rejects them before publication without silently changing existing ShaderRef spelling

#### Scenario: Include dependency is undeclared
- **WHEN** an include requests a resource outside the owner and its declared resource dependencies or escapes its allowed root
- **THEN** resolution fails explicitly rather than searching another owner's files

#### Scenario: Custom source and include roots
- **WHEN** a module configures a non-default physical root or provider with the prescribed namespace
- **THEN** compilation, dependency collection and cook resolve the same canonical inputs without changing ShaderRef identity

#### Scenario: Reversed mount order
- **WHEN** the same validated non-conflicting provider set is composed in another order
- **THEN** every shader/include resolves to the same provider and content

#### Scenario: Local include resolution
- **WHEN** a shader uses a quoted local include
- **THEN** its logical directory and permitted include roots resolve one canonical candidate, or report ambiguity, without cross-owner basename fallback

#### Scenario: Macro includes and module imports
- **WHEN** a shader uses macro-expanded includes or Slang module imports
- **THEN** dependency identity covers those inputs through compiler-observed dependencies or a conservative complete declared closure, and changing their contents invalidates dependent results

### Requirement: Runtime and cook share owner resource descriptions

Development compilation and offline cook SHALL consume the same collected resource descriptions, logical identities and dependency contents. The existing project cook/package entry SHALL collect shader requirements from material techniques and explicit FullScreenPass/ComputePass techniques, include compiled products, reflection, schema and required variants in selected distributions, and provide runtime lookup without per-module manual copy/cook steps. Product lookup SHALL retain all selected owners' shader records rather than rely on first-match per-module cache indices.

#### Scenario: Staged sources match development inputs
- **WHEN** an owner/dependency graph is compiled from native development files and equivalent staged cook inputs
- **THEN** both resolve the same logical sources, schema and dependency contents and produce compatible target reflection

#### Scenario: Source distribution contains exported interfaces
- **WHEN** a module's shader sources are staged for a source or SDK distribution
- **THEN** its persisted `.slangh` files are collected with `.slang` and companion assets from the same registered root, preserving include paths without requiring the consumer to regenerate headers

#### Scenario: Shared dependency edit
- **WHEN** an exported header or declared dependency changes
- **THEN** affected build outputs and runtime/cook cache identities refresh without conflating another owner's same-basename resource

#### Scenario: Unified project packaging
- **WHEN** a project packages selected Aurora, plugin and project contributions through normal cook/package entry points
- **THEN** shader requests from scene materials and explicit fullscreen/compute techniques enter the selected distribution without separate shader deployment steps

#### Scenario: Several owners provide cache records
- **WHEN** two selected plugins contribute shader records for the same backend
- **THEN** the distributed index contains both owners' requests and blobs, and conflicts fail explicitly rather than one plugin's index hiding the other

#### Scenario: Relocated package without shader sources
- **WHEN** a packaged project runs after relocation with development source roots unavailable and runtime Slang absent
- **THEN** required scene, fullscreen and compute requests resolve from shipped products without embedded-source compilation fallback

### Requirement: Compilation retains its source snapshot

Accepted compilation SHALL retain a stable owned source/dependency view, inventory and required provider/module leases until completion or collected cancellation. Resource registration/retirement SHALL NOT mutate or destroy backing still used by a compile job. Failures SHALL release borrows through the same lifetime protocol.

#### Scenario: Source owner retires during compilation
- **WHEN** an owner stops accepting requests while a shader job is in flight
- **THEN** the job finishes against its retained snapshot or reports collected cancellation, and ownership is released only after that use ends
