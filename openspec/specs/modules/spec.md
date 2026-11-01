# modules Specification

## Purpose

Define module registration, plugin boundaries, runtime configuration and static or dynamic packaging.

## Scope notes

### module-registration

Define the module lifecycle contract that separates **process-once metadata registration** from the repeatable
per-load lifecycle. Process-global metadata (reflection types, asset types/handlers, component groups, cook
builders) is registered exactly once per process per module, so module reload and multi-DLL loading cannot duplicate
it; per-load registration is reserved for services that are torn down on unload.

## Requirements

### Requirement: Engine builtin configuration has one authoritative location

Repository-level engine builtin configuration SHALL live in the repository-root `configs/` directory. A duplicate `engine/configs/` tree SHALL NOT exist.

#### Scenario: Only the root configs directory exists
- **WHEN** the repository layout is inspected
- **THEN** engine builtin configuration files SHALL be present under `configs/`, and `engine/configs/` SHALL NOT exist

#### Scenario: Duplicate file removed
- **WHEN** the former `engine/configs/mesh_build_presets.json` is compared with the root file
- **THEN** the root `configs/mesh_build_presets.json` SHALL remain unchanged and the duplicate SHALL be removed

### Requirement: Build and packaging consume the authoritative configs directory

CMake configuration, editor and game deployment, and Python packaging SHALL read engine builtin configuration from the repository-root `configs/` directory.

#### Scenario: Editor deployment reads root configs
- **WHEN** `SandboxEditor` is built
- **THEN** its deployed `configs/` SHALL be copied from the repository-root `configs/`

#### Scenario: Game deployment reads root configs first
- **WHEN** a generated game project deploys configuration
- **THEN** it SHALL deploy the repository-root engine `configs/` first and the project `configs/` second, preserving project override precedence

#### Scenario: Packaging reads root configs
- **WHEN** `python/package_project.py` stages configuration
- **THEN** it SHALL copy from the repository-root `configs/` and the project `configs/` in that order

#### Scenario: Editor module manifest resolves
- **WHEN** top-level CMake reads the editor module manifest
- **THEN** the path SHALL resolve to the repository-root `configs/modules_editor.json`

### Requirement: Engine modules expose interfaces and data only

An engine feature's contract target SHALL contain only interface and referenced data definitions, and pure functions over that data; it SHALL NOT contain or depend on the feature's concrete implementation. Existing engine foundation modules, including UI, SHALL retain their established engine ownership and MAY separate concrete implementations into targets within the same `engine/<foundation>` module according to responsibility. Foundation ownership SHALL NOT weaken contract-target isolation: `UI.Interface` SHALL remain contracts, referenced data and pure utilities only, without including or linking UI implementation targets. This change SHALL NOT alter the established ownership of unrelated features.

#### Scenario: Engine contract target has no implementation
- **WHEN** an engine feature's contract target is inspected
- **THEN** it SHALL expose interfaces, referenced data types and pure utilities only, with no concrete feature implementation or dependency on implementation targets

#### Scenario: Existing foundation keeps separated implementations in engine
- **WHEN** an existing engine foundation such as UI separates runtime, editor and adaptor responsibilities into targets
- **THEN** its concrete implementation SHALL be allowed within its engine module while its contract target remains independent of those implementations

### Requirement: Features are implemented as plugins

A domain feature SHALL by default provide its concrete implementation in a plugin under `plugins/`, which implements its engine contract. Existing engine foundations such as UI SHALL be allowed to retain concrete implementations in their established engine module through separated targets and SHALL NOT require a feature-plugin entry merely to satisfy this default. This foundation rule SHALL NOT relocate unrelated feature implementations or alter existing domain-specific ownership requirements.

#### Scenario: Plugin implements the engine interface
- **WHEN** a domain feature plugin is loaded
- **THEN** it SHALL provide the concrete implementation registered with the engine interface

#### Scenario: Engine does not depend on a plugin
- **WHEN** an engine module is compiled and linked
- **THEN** it SHALL NOT link or include a plugin's implementation

#### Scenario: Foundation implementation does not require a plugin
- **WHEN** UI foundation implementation targets are configured within `engine/ui`
- **THEN** those targets SHALL NOT require a UI feature-plugin entry, and unrelated feature ownership SHALL remain unchanged

### Requirement: Consumers depend on interfaces

Consumers SHALL depend on the engine interfaces and resolve the concrete implementation at runtime (world sub-system by name), never on a plugin's concrete types.

#### Scenario: Consumer resolves by name

- **WHEN** a consumer needs the feature at runtime
- **THEN** it SHALL obtain the implementation through the engine interface (e.g. the world sub-system by name)

#### Scenario: Feature absent is handled

- **WHEN** the feature plugin is not present in a world
- **THEN** the consumer SHALL treat the feature as unavailable rather than fail to build

### Requirement: Terrain implementation lives in a plugin

The terrain feature SHALL keep only interfaces and the data those interfaces reference (`TerrainMeta`, tile coordinates/info/format, addressing, region sink/change listener, `ITerrainField`/`ITerrainSystem`) in `engine/terrain`, and SHALL implement the field, sub-system, procedural generation, source, asset payload/traits + serialization/registration, LOD description, asset builder, collision layer, component, and builder module in `plugins/terrain`.

#### Scenario: Engine terrain module is interface/data only

- **WHEN** `engine/terrain` is compiled
- **THEN** it SHALL expose only terrain interfaces/data and SHALL NOT link the terrain implementation

#### Scenario: Terrain plugin implements the interfaces

- **WHEN** `plugins/terrain` is built
- **THEN** it SHALL implement `ITerrainSystem`/`ITerrainField` and register the terrain component and asset handling

### Requirement: Vegetation implementation lives in a plugin

The vegetation feature SHALL keep only the surface provider/listener seam, the data referenced by the system interface, and `IVegetationSystem` in `engine/vegetation`, and SHALL implement placement, the sub-system, asset payload/traits + serialization/registration, component, terrain surface bridge, render, and editor in `plugins/vegetation`.

#### Scenario: Engine vegetation module is interface/data only

- **WHEN** `engine/vegetation` is compiled
- **THEN** it SHALL expose only vegetation interfaces/data and SHALL NOT link the vegetation implementation

#### Scenario: Vegetation plugin implements the interfaces

- **WHEN** `plugins/vegetation` is built
- **THEN** it SHALL implement the vegetation sub-system and providers and register the vegetation component and asset handling

### Requirement: Stable include paths during the restructure

The restructure SHALL keep the public include paths (`terrain/...`, `vegetation/...`) stable so moving implementation files does not churn consumers' includes.

#### Scenario: Includes unchanged

- **WHEN** implementation files move from `engine/` to `plugins/`
- **THEN** consumer include directives SHALL remain valid

### Requirement: UI engine contracts are separate from toolkit implementation

UI SHALL be an explicitly authorized engine foundation exception: its implementation SHALL remain within `engine/ui`, separated by target responsibility. The root `UI.Interface` target SHALL contain only provider interfaces, referenced data, render-agnostic draw/layout/property data and pure utilities. Concrete retained elements, widgets, input routing, document schema/loading/bindings, text runtime, animation and localization SHALL live in `engine/ui/core`; document editing in `engine/ui/editor`; Aurora rendering in `engine/ui/render`; and the optional FreeType provider in `engine/ui/freetype`. `UI.Interface` SHALL NOT include or link those implementation targets. UI SHALL NOT require a feature-plugin entry, and this exception SHALL NOT alter ownership rules for unrelated features.

#### Scenario: Contract target stays independent within the foundation
- **WHEN** root `UI.Interface` source, public headers and target dependencies are inspected
- **THEN** that target SHALL contain only contracts/data/pure utilities and SHALL NOT include widget, document-loader, renderer, document-editor or font-rasterizer implementation

#### Scenario: UI foundation dependencies point toward core and interfaces
- **WHEN** the UI foundation target dependencies are inspected
- **THEN** editor and adaptor targets SHALL depend toward UI runtime or `UI.Interface`, and UI runtime SHALL NOT depend on editor or adaptor targets

#### Scenario: Public domain include paths survive relocation
- **WHEN** a consumer uses the separated targets under the `engine/ui` foundation
- **THEN** consumers linking the appropriate runtime, editor, or adaptor target SHALL retain their `ui/...` public includes

#### Scenario: Foundation implementation is allowed in engine UI
- **WHEN** the UI foundation's placement is reviewed against the general feature layering rule
- **THEN** `core`, `editor`, `render` and `freetype` implementations SHALL be allowed within `engine/ui`, while their one-way target dependencies remain enforced

### Requirement: UI foundation implementation remains in its engine module

UI SHALL keep its runtime, render, optional FreeType and document-editor implementations in `engine/ui/{core,render,freetype,editor}`. This explicit foundation ownership SHALL retain independent targets and one-way dependencies toward runtime/contracts, with no UI feature-plugin registration. Existing target names and `ui/...` public includes SHALL remain available.

#### Scenario: Build consumes relocated foundation targets
- **WHEN** a host selects UI, optional FreeType or document editing
- **THEN** CMake SHALL use the matching engine/ui targets without requiring plugins/ui, and runtime SHALL NOT depend on editor/render/font implementations

#### Scenario: Renderer implementation is relocated
- **WHEN** UI rendering sources move back to the foundation
- **THEN** relocation SHALL preserve implementation bytes and SHALL NOT change renderer resources, submissions or backend behavior

### Requirement: Module packaging mode selection

The engine SHALL support two runtime module packaging modes - **dynamic** (one shared library per module) and
**static bundling** (selected modules linked into the host/bundle) - and SHALL default to **dynamic** so existing
projects are unchanged.

#### Scenario: Default is dynamic
- **WHEN** a project does not specify a packaging mode
- **THEN** every runtime module is packaged as a dynamic (shared) library

#### Scenario: Static mode selected
- **WHEN** the packaging mode is set to static
- **THEN** each module's static core is linked into the host/bundle and no separate shared library is emitted for it

### Requirement: Static bundling rules

Static bundling SHALL be driven by an explicit rule set (which modules are bundled, and any grouping) rather than by
editing individual modules, and SHALL allow a mixed mode where only the listed modules are static.

#### Scenario: Only listed modules are bundled
- **WHEN** the mode is mixed and a rule set lists specific modules
- **THEN** exactly those modules are statically bundled and all others remain dynamic

#### Scenario: Editor-only modules excluded
- **WHEN** a runtime bundle is produced
- **THEN** modules marked editor-only are not pulled into the static bundle

### Requirement: Dynamic registration behavior is preserved

A statically bundled module SHALL still register with `ModuleManager` at startup; static bundling removes the shared
library, not the registration.

#### Scenario: Statically bundled module registers without a library
- **WHEN** a module is statically bundled
- **THEN** it is registered at startup through a compiled-in registration and no dynamic library is required

#### Scenario: No double registration
- **WHEN** a module is statically bundled and a shared library of the same module is also present
- **THEN** the runtime registers the module exactly once, with the static instance taking precedence

### Requirement: Packaging options are preset-driven

The packaging mode and static bundling rules SHALL be controllable through build/packaging options and SHALL be
exposed through the project export preset, consistent with the project packaging option surface.

#### Scenario: Preset selects the mode and rules
- **WHEN** a project export preset sets the packaging mode and the static module rules
- **THEN** the generated/build project honors them during configure and packaging

### Requirement: Process-once module metadata registration

The module lifecycle SHALL provide a process-once registration phase, `IModule::RegisterMeta()`, distinct from the
repeatable load lifecycle. The module host SHALL invoke it at most once per process per module, and SHALL NOT invoke
it again when a module is reloaded.

#### Scenario: Reload does not re-register

- **WHEN** a module is loaded, unloaded, and loaded again in the same process
- **THEN** its `RegisterMeta()` SHALL have run exactly once, and process-global metadata SHALL NOT be registered a second time

#### Scenario: Registration happens before first use

- **WHEN** a module is loaded for the first time
- **THEN** `RegisterMeta()` SHALL run before `Init()` for that module

### Requirement: Process-global metadata is registered in the process-once phase

Module-owned process-global metadata that persists for the process lifetime and is not undone on unload SHALL be registered in RegisterMeta, not in Init/Start/Shutdown. This includes module reflection types, asset types and asset handlers, component groups, and offline cook builders. Built-in Core metadata SHALL instead have one owner in creation of the process-shared serialization context; its factory SHALL receive that context explicitly and SHALL NOT recursively resolve the singleton during construction. Applications and runtime sessions SHALL consume that existing metadata without repeating reflection, using registration guards, or clearing the shared Environment.

#### Scenario: Reflection registration is process-once

- **WHEN** a module registers reflection types / asset handlers / component groups / cook builders
- **THEN** those registrations SHALL be performed in RegisterMeta and SHALL be present after the module is unloaded

#### Scenario: Built-in context survives application recreation

- **WHEN** multiple application sessions use the process-shared serialization context in one process
- **THEN** its creation owner SHALL initialize built-in Core metadata exactly once with the explicit context instance
- **AND** later sessions SHALL consume the same definitions without re-registering them or resetting the Environment

### Requirement: Per-load registration is undone on unload

Registration that is torn down in `Shutdown()` SHALL remain on the per-load path (`Init`/`Start`), so that
load/unload cycles stay symmetric. This includes backends, factories, world subsystems, and similar runtime
services.

#### Scenario: Runtime services are symmetric across reload

- **WHEN** a module that registers a runtime service is unloaded and reloaded
- **THEN** the service SHALL be unregistered on unload and re-registered on the next load without duplication

### Requirement: Duplicate registration is a hard error, not silently tolerated

The process-global registries SHALL NOT be made idempotent to hide a re-registration; a registration performed more
than once SHALL remain an assertion failure, so a misplaced persistent registration is caught rather than masked.

#### Scenario: Misplaced registration fails loudly

- **WHEN** a persistent, process-global registration is mistakenly performed on the per-load path twice
- **THEN** the duplicate-registration assertion SHALL fail rather than be tolerated

### Requirement: One source owner per metadata definition

Every reflected data definition SHALL have exactly one production registration owner. Runtime, cook, editor and window consumers SHALL depend on that owner and SHALL NOT invoke its reflection function themselves. Reflection helpers SHALL NOT use Once, once_flag, static flags, existing-type early returns or scratch nodes to tolerate duplicate calls.

#### Scenario: Shared asset schema across runtime cook and editor

- **WHEN** runtime, cook and editor modules for the same asset domain are loaded together
- **THEN** their shared metadata owner SHALL register the schema once
- **AND** every consumer SHALL use that definition without re-registering it

#### Scenario: Reopening a reflected editor

- **WHEN** several material, reflection demo or settings views are created and closed
- **THEN** their construction and binding SHALL perform no metadata registration

### Requirement: Process definitions are distinct from runtime sessions

The module host SHALL own process module definitions independently of repeatable runtime sessions. Only definition creation SHALL invoke RegisterMeta. Init failure, module reload or a new ModuleManager SHALL preserve the existing definition. A duplicate UUID or registered name from another owner SHALL remain a programming error.

#### Scenario: Failed initialization is retried by another manager

- **WHEN** a module finishes metadata registration, fails Init and is retried by another manager
- **THEN** the new runtime session SHALL reuse its existing process definition
- **AND** RegisterMeta SHALL have been called exactly once
- **AND** each failed or unloaded runtime instance SHALL still be shut down and destroyed

### Requirement: Loader owns process-resident callback code

The loader SHALL keep successfully loaded DLLs resident until process exit, including runtime-only modules. Metadata definitions and callback registries SHALL NOT require per-object code tokens. Runtime shutdown SHALL withdraw services and drain accepted work before destroying their data and provider dependencies. Binary replacement or removal SHALL take effect after process restart; code residency SHALL NOT establish initialized-session readiness.

#### Scenario: Metadata survives session reload

- **WHEN** cook and editor runtime sessions are unloaded and recreated
- **THEN** existing reflected objects and builders SHALL remain usable with stable definition identity
- **AND** the underlying shared Environment SHALL remain attached

#### Scenario: Runtime-only compiler session stops

- **WHEN** a shader compiler module is shut down and its final external compile request is released
- **THEN** its compiler instance SHALL be released when its last real owner exits
- **AND** the DLL SHALL remain resident until process exit without a code lease

### Requirement: Tests have explicit metadata initialization owners

Tests SHALL initialize each production or fixture type through its explicit executable, test-environment or production context owner before consuming it. Built-in Core types SHALL be consumed from the process-shared serialization context owner. Per-test setup, getters and constructors SHALL NOT repeat reflection or use registration guards. A fixture factory that initializes a context SHALL receive that context explicitly rather than recursively requesting it from its singleton constructor.

#### Scenario: Several tests consume the same production types

- **WHEN** a suite contains multiple tests using Aurora or Content data
- **THEN** the executable or test environment SHALL register that data once before test execution

#### Scenario: Tests consume built-in Core metadata

- **WHEN** several tests or recreated application sessions use built-in Core types
- **THEN** they SHALL consume the definitions owned by creation of the process-shared serialization context
- **AND** they SHALL NOT invoke Core reflection again, add an idempotence guard, or clear the Environment between consumers

### Requirement: Shared metadata module consumers

Aurora.Metadata, Content.Metadata and Animation.GraphAssets SHALL own their shared reflection and persistent asset handlers. Runtime, builder and editor modules SHALL declare dependencies on the matching owner. Content.Module SHALL publish the Clip source service through Init/Shutdown and SHALL NOT call ContentReflection. Builder registration SHALL have one process definition owner and retain its callback code; runtime sessions SHALL NOT repeat registration.

#### Scenario: Content runtime and cook coexist
- **WHEN** Content.Module and Content.Builder.Module load in one process
- **THEN** Content.Metadata registers the shared Clip schema and handler once, while the builder module registers its builder once through RegisterMeta
