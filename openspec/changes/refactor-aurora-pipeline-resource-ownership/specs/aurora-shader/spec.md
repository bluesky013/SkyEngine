## ADDED Requirements

### Requirement: Owner-scoped shared declarations

Shared declaration helpers SHALL allow each pipeline owner to select a distinct C++ namespace while consuming one shared `.h` source from C++ and shaders. User plugins SHALL retain ownership of their headers and shader implementations and SHALL NOT require modifying engine source lists or running Slang to compile their C++ consumers. Global/Pass/Batch SHALL remain binding tiers with owner-defined layouts and optional View.

#### Scenario: Two plugins reuse local names
- **WHEN** two independently built plugins both declare ToonPass and ToonParams in their own namespaces
- **THEN** their shared headers compile together without symbol/ODR collisions and each shader reflects its owner's declared layout

#### Scenario: External pipeline omits View
- **WHEN** a user plugin declares a Pass without camera data and uses its own Batch layout
- **THEN** it compiles and binds without depending on built-in ScenePass, SceneBatch or GlobalBlock layouts

### Requirement: Qualified owner shader bundles

Each shader bundle SHALL have a stable owner identity, qualified logical paths and explicit dependency owners. Bundle inputs SHALL be declared by the owning target and resolve through the existing shader filesystem mechanisms independently of source/install/package locations. Conflicting paths or owner registrations SHALL return explicit errors without partially publishing a bundle. Resolution SHALL NOT silently choose among owners by search-path priority.

#### Scenario: Same basename in separate owners
- **WHEN** two plugins each package a local ToonPass.h
- **THEN** owner-qualified includes resolve to the intended file regardless of registration order or physical package location

#### Scenario: Conflicting registration is atomic
- **WHEN** a bundle attempts to publish a conflicting owner or fully qualified path
- **THEN** registration fails explicitly and previously published resources remain unchanged

#### Scenario: Include dependency is undeclared
- **WHEN** an include requests a resource outside the owner and its declared dependencies, or escapes its allowed logical root
- **THEN** resolution fails with an explicit path/dependency error instead of falling through to another plugin's file

### Requirement: Runtime and cook share owner resource descriptions

Runtime compilation and offline shader cook SHALL consume the same owner bundle descriptions, logical identities and dependency contents. Cook SHALL NOT require loading a pipeline runtime DLL or editor. Source, packaged and embedded representations SHALL resolve equivalent inputs, and shared dependency changes SHALL invalidate relevant build/source-closure cache results using existing cache mechanisms.

#### Scenario: Packaged shader matches development compilation
- **WHEN** the same owner/dependency graph is compiled from development files at runtime and from packaged inputs in offline cook
- **THEN** both resolve the same logical sources, schema and dependency contents and produce compatible target reflection

#### Scenario: Shared helper edit invalidates dependent shaders
- **WHEN** a declared dependency header changes
- **THEN** affected owner bundles refresh and runtime/cook reject stale shader cache results without conflating another owner's same-basename resource

### Requirement: Compilation retains its source snapshot

An accepted compilation SHALL retain a stable owned source/dependency view until completion or explicit cancellation has been collected. Resource registration or retirement SHALL NOT mutate or destroy source backing or provider code still used by a compile job. Failures SHALL release these borrows through the same lifetime protocol.

#### Scenario: Source owner retires during compilation
- **WHEN** a plugin source owner stops accepting requests while a shader job is in flight
- **THEN** the job finishes against its retained snapshot or reports collected cancellation, and its source/provider ownership is released only after that use ends
