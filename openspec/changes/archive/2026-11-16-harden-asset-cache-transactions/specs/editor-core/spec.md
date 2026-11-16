## ADDED Requirements

### Requirement: Undo commands validate document and object incarnations
World editing commands SHALL validate document session, world identity and runtime instance, actor identity, component type and component incarnation before editing or invoking dirty callbacks. This applies to property, actor, creation, add-component and skeletal commands. Retained objects may participate in valid detach/reattach cycles; replacement objects at the same address or UUID are distinct incarnations.

#### Scenario: A command outlives its document session
- **WHEN** a command is invoked after the document is rebound or closed
- **THEN** it fails explicitly without editing or marking another document dirty

#### Scenario: A component is replaced
- **WHEN** an actor receives a new component with the same type
- **THEN** an old command cannot mutate the replacement component

### Requirement: Reflected defaults preserve root ownership
The property form SHALL own a default-constructed reflected baseline through its registered constructor and deleter without requiring the root to be copyable. Field reset and undo SHALL preserve the live object's identity.

#### Scenario: Inspect a non-copyable polymorphic component
- **WHEN** the form binds a non-copyable polymorphic root with editable fields
- **THEN** it captures field defaults without copying the root or destroying unconstructed storage
- **AND** resetting and undoing a field preserves the live root and releases the temporary baseline exactly once
