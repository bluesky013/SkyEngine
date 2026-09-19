## ADDED Requirements

### Requirement: Document editing operations
The editor SHALL support finding elements by name and adding, removing, renaming, and reordering elements,
using the element-type registry to create new elements.

#### Scenario: Add child
- **WHEN** a child of a registered type is added under a parent
- **THEN** it SHALL appear in the parent's children

#### Scenario: Remove by name
- **WHEN** an element is removed by name
- **THEN** it SHALL no longer be found in the tree

#### Scenario: Reorder siblings
- **WHEN** an element is moved up or down among its siblings
- **THEN** its position in the child order SHALL change by one

### Requirement: Serialization round-trip
The editor SHALL serialize the tree to the UI document format and load it back without losing the element
hierarchy, type, or name.

#### Scenario: Round-trip preserves structure
- **WHEN** a tree is serialized and then deserialized
- **THEN** the reloaded tree SHALL have the same element types and names

### Requirement: Undo and redo
The editor SHALL snapshot the document before each edit and SHALL restore prior and subsequent states through
undo and redo.

#### Scenario: Undo restores previous state
- **WHEN** an edit is undone
- **THEN** the document SHALL return to the state before the edit

#### Scenario: Redo reapplies
- **WHEN** a redo follows an undo
- **THEN** the edit SHALL be reapplied
