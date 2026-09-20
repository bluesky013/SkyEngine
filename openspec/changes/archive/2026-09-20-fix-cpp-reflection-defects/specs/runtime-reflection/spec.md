## ADDED Requirements

### Requirement: Registered members bind to their declared fields

Runtime reflection SHALL expose each registered member with its declared type, and registration SHALL bind the
member to the field it names. A registered member SHALL read and write the same underlying field.

#### Scenario: Color alpha is reflected

- **WHEN** a `Color` value is accessed through reflection
- **THEN** the member named `a` SHALL read and write `Color::a`, and no member SHALL alias `Color::b` twice

#### Scenario: Editor scalar shows the declared value

- **WHEN** a reflected member of a numeric type is displayed in an editor widget
- **THEN** the widget SHALL read and display the value using that member's declared type

#### Scenario: Editor does not attach to unhandled members

- **WHEN** a member type has no editor widget
- **THEN** no widget SHALL be created and no signal SHALL be connected for that member

### Requirement: Any preserves value semantics safely

`Any` SHALL own and copy/move its contained value without leaking, double-freeing, or corrupting data, including
self-assignment and non-copyable small types.

#### Scenario: Copy assignment releases the previous value

- **WHEN** an `Any` holding a large value is copy-assigned
- **THEN** the previous value's storage SHALL be released before the new value is stored

#### Scenario: Self-assignment is safe

- **WHEN** an `Any` is assigned from itself
- **THEN** its value SHALL remain unchanged and valid

#### Scenario: Move does not leak

- **WHEN** an `Any` holding a large value is move-constructed or move-assigned
- **THEN** no intermediate allocation SHALL be left unreleased

### Requirement: Sequence containers round-trip through archives

Reflection SHALL report a valid element type for a sequence member, and sequence members SHALL survive a
serialize/deserialize round-trip through both the JSON and Binary archives.

#### Scenario: Element type of an empty container

- **WHEN** the element type of a sequence member is queried
- **THEN** it SHALL return the registered element type without dereferencing null container metadata

#### Scenario: Sequence round-trip

- **WHEN** a type with a sequence member is saved and loaded
- **THEN** the reloaded sequence SHALL contain the same elements in the same order

### Requirement: Binary archive supports enums and sequences

The Binary archive SHALL serialize enum members by their underlying type and SHALL serialize sequence members
symmetrically with the JSON archive, rather than silently skipping them.

#### Scenario: Enum binary round-trip

- **WHEN** a type with an enum member is saved and loaded through the Binary archive
- **THEN** the enum value SHALL be restored

#### Scenario: No silent data loss

- **WHEN** a reflected member cannot be serialized by an archive
- **THEN** the archive SHALL report the condition instead of silently omitting the member

### Requirement: Member function invocation validates arguments

Invoking a reflected member function SHALL verify that the supplied argument count and types match the function
before dispatching, and SHALL reject a mismatch.

#### Scenario: Argument mismatch is rejected

- **WHEN** a reflected member function is invoked with the wrong argument count or type
- **THEN** the invocation SHALL fail instead of dispatching with invalid arguments
