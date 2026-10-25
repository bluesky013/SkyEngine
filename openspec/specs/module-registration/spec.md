# module-registration Specification

## Purpose

Define the module lifecycle contract that separates **process-once metadata registration** from the repeatable
per-load lifecycle. Process-global metadata (reflection types, asset types/handlers, component groups, cook
builders) is registered exactly once per process per module, so module reload and multi-DLL loading cannot duplicate
it; per-load registration is reserved for services that are torn down on unload.
## Requirements
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

Process-global metadata that persists for the process lifetime and is not undone on unload SHALL be registered in
`RegisterMeta()`, not in `Init`/`Start`/`Shutdown`. This includes reflection types, asset types and asset handlers,
component groups, and offline cook builders.

#### Scenario: Reflection registration is process-once

- **WHEN** a module registers reflection types / asset handlers / component groups / cook builders
- **THEN** those registrations SHALL be performed in `RegisterMeta()` and SHALL be present after the module is unloaded

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

### Requirement: Metadata owns callback code lifetime

Metadata definitions and callback registries SHALL retain their code while callbacks, type descriptors, builders or handlers remain callable. Sessions sharing a loaded DLL SHALL reuse its lease so a session cannot detach the library while another owner remains. Definitions without persistent callbacks SHALL NOT permanently retain runtime-only DLLs.

#### Scenario: Metadata survives session reload

- **WHEN** cook and editor runtime sessions are unloaded and recreated
- **THEN** existing reflected objects and builders SHALL remain usable with stable definition identity
- **AND** the underlying shared Environment SHALL remain attached

#### Scenario: Runtime-only compiler can unload

- **WHEN** a shader compiler module is shut down and its final external compile request is released
- **THEN** its module definition SHALL NOT prevent the DLL from unloading

### Requirement: Tests have explicit metadata initialization owners

Tests SHALL initialize each production or fixture type through an executable or test-environment owner before consuming it. Per-test setup, getters and constructors SHALL NOT repeat reflection or use registration guards.

#### Scenario: Several tests consume the same production types

- **WHEN** a suite contains multiple tests using Aurora or Content data
- **THEN** the executable or test environment SHALL register that data once before test execution

### Requirement: Shared metadata module consumers

Aurora.Metadata and Content.Metadata SHALL own their existing shared reflection and persistent asset handlers. Runtime, builder and editor modules SHALL declare dependencies on the matching owner and SHALL NOT call shared reflection. Builder registration SHALL have one process definition owner and retain its callback code; runtime sessions SHALL NOT repeat registration. Asset-domain changes and new domain-specific owners SHALL migrate their consumers together with their implementation.

#### Scenario: Content runtime and cook coexist
- **WHEN** Content.Module and Content.Builder.Module load in one process
- **THEN** Content.Metadata registers the existing Content schemas and handlers once, while the builder module registers its builder once through RegisterMeta
