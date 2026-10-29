## MODIFIED Requirements

### Requirement: Process-global metadata is registered in the process-once phase

Module-owned process-global metadata that persists for the process lifetime and is not undone on unload SHALL be registered in RegisterMeta, not in Init/Start/Shutdown. This includes module reflection types, asset types and asset handlers, component groups, and offline cook builders. Built-in Core metadata SHALL instead have one owner in creation of the process-shared serialization context; its factory SHALL receive that context explicitly and SHALL NOT recursively resolve the singleton during construction. Applications and runtime sessions SHALL consume that existing metadata without repeating reflection, using registration guards, or clearing the shared Environment.

#### Scenario: Reflection registration is process-once

- **WHEN** a module registers reflection types / asset handlers / component groups / cook builders
- **THEN** those registrations SHALL be performed in RegisterMeta and SHALL be present after the module is unloaded

#### Scenario: Built-in context survives application recreation

- **WHEN** multiple application sessions use the process-shared serialization context in one process
- **THEN** its creation owner SHALL initialize built-in Core metadata exactly once with the explicit context instance
- **AND** later sessions SHALL consume the same definitions without re-registering them or resetting the Environment

### Requirement: Tests have explicit metadata initialization owners

Tests SHALL initialize each production or fixture type through its explicit executable, test-environment or production context owner before consuming it. Built-in Core types SHALL be consumed from the process-shared serialization context owner. Per-test setup, getters and constructors SHALL NOT repeat reflection or use registration guards. A fixture factory that initializes a context SHALL receive that context explicitly rather than recursively requesting it from its singleton constructor.

#### Scenario: Several tests consume the same production types

- **WHEN** a suite contains multiple tests using Aurora or Content data
- **THEN** the executable or test environment SHALL register that data once before test execution

#### Scenario: Tests consume built-in Core metadata

- **WHEN** several tests or recreated application sessions use built-in Core types
- **THEN** they SHALL consume the definitions owned by creation of the process-shared serialization context
- **AND** they SHALL NOT invoke Core reflection again, add an idempotence guard, or clear the Environment between consumers
