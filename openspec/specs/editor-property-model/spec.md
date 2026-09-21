# editor-property-model Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
### Requirement: Reflection-backed property descriptors
The property model SHALL expose, for a reflected object, descriptors derived from the C++ reflection data
(`serialize::TypeMemberNode`) that report name, display name, category, type, and metadata, and that can read and
write the member value for a given object handle.

#### Scenario: Read and write a member
- **WHEN** a descriptor for a scalar member is used to get and then set the value on an object
- **THEN** the get SHALL return the current value and the set SHALL update the member

#### Scenario: Descriptor carries metadata
- **WHEN** a descriptor is queried
- **THEN** it SHALL report at least its name, display name, category, and type

### Requirement: Child descriptors
The model SHALL expose child descriptors for sequence members (one per element) and SHALL expose the nested
`TypeNode` for struct members so a view can build a sub-model. Associative maps are out of scope for this change.

#### Scenario: Sequence element exposed
- **WHEN** an object has a sequence member with N elements
- **THEN** the model SHALL provide a child descriptor for each element

#### Scenario: Struct member exposes its type
- **WHEN** a member's value is a reflected struct
- **THEN** the descriptor SHALL expose the struct's `TypeNode`

### Requirement: View independence and undo integration
The property model SHALL be usable without any UI toolkit and SHALL route value changes through the undo/redo
service.

#### Scenario: Edit is undoable
- **WHEN** a value is changed through the property model
- **THEN** the change SHALL be recorded as an undoable command

#### Scenario: Model used headlessly
- **WHEN** the model is exercised in a test without a window or UI toolkit
- **THEN** it SHALL read and write values successfully

