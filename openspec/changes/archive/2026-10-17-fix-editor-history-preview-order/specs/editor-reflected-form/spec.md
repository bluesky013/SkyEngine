## ADDED Requirements

### Requirement: End interactions before shared history traversal

Reflected forms SHALL subscribe to the command service supplied by their context and cancel live interactions before that service traverses history. Shell accelerators, shared toolbar/menu actions, and direct service calls SHALL use this ordering. Forms SHALL remove preparation subscriptions on destruction and guard copied callbacks against expired lifetime. Binding invalidation SHALL abandon invalid targets before they can receive a cancellation callback.

#### Scenario: Shared entry points preserve history values
- **WHEN** an asset or standalone form has committed A to B and is previewing C and Undo is invoked by a Shell accelerator, shared menu/toolbar action, or direct command-service call
- **THEN** the preview SHALL end before Undo restores A
- **AND** later Escape, pointer release, or layout synchronization SHALL preserve A and the advanced history cursor

#### Scenario: Form removed during preparation
- **WHEN** an earlier preparation observer destroys a form
- **THEN** later callbacks SHALL NOT access that form or its old binding

#### Scenario: Local history while inline editing
- **WHEN** the form receives a key-down history shortcut during inline editing
- **THEN** it SHALL route through the same service preparation boundary
- **AND** the corresponding key-up SHALL NOT traverse history again
