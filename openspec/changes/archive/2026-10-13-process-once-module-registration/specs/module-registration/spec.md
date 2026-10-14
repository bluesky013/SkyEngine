## ADDED Requirements

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
