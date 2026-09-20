## ADDED Requirements

### Requirement: Registered types are exposed to Python

The binding layer SHALL create a Python type for each type registered in the runtime reflection context, resolved
lazily by name, without per-type hand-written binding code.

#### Scenario: Look up a registered type

- **WHEN** a script requests a registered type by name
- **THEN** it SHALL receive a Python type representing that C++ type

#### Scenario: Unknown type

- **WHEN** a script requests a type name that is not registered
- **THEN** the lookup SHALL raise a Python exception naming the missing type

### Requirement: Instances can be constructed

The binding layer SHALL construct reflected instances using the registered constructors, including the default
constructor.

#### Scenario: Default construction

- **WHEN** a script constructs a type that has a default constructor
- **THEN** it SHALL receive an instance wrapping a default-constructed value

#### Scenario: Argument construction

- **WHEN** a script constructs a type with arguments matching a registered constructor
- **THEN** it SHALL receive an instance constructed from those arguments

### Requirement: Member access reflects C++ fields

The binding layer SHALL expose reflected members as readable and, when writable, assignable Python attributes
that read and write the same underlying C++ field.

#### Scenario: Read a member

- **WHEN** a script reads a member of an instance
- **THEN** it SHALL receive the current value of the underlying field

#### Scenario: Write a member

- **WHEN** a script assigns a writable member of an instance
- **THEN** the underlying field SHALL be updated

#### Scenario: Const member is read-only

- **WHEN** a script assigns a member that is registered as const
- **THEN** the assignment SHALL fail and SHALL NOT modify the underlying field

#### Scenario: Unknown member

- **WHEN** a script reads or writes an attribute that is not a reflected member
- **THEN** the access SHALL raise a Python exception

### Requirement: Enums are exposed

The binding layer SHALL expose reflected enumeration types together with their declared names and values.

#### Scenario: Enum values are accessible

- **WHEN** a script accesses a reflected enum's declared value
- **THEN** it SHALL receive the value associated with that name

#### Scenario: Enum member round-trips

- **WHEN** a script reads an enum-typed member and writes it back
- **THEN** the underlying enum field SHALL retain the same value

### Requirement: Sequence containers are exposed and mutable

The binding layer SHALL expose reflected sequence members as Python sequence objects backed by the underlying
container, supporting length, indexed access, and mutation.

#### Scenario: Length and indexing

- **WHEN** a script queries the length of a sequence member or indexes an element
- **THEN** it SHALL observe the underlying container's size and elements

#### Scenario: Append and erase

- **WHEN** a script appends to or erases from a sequence member
- **THEN** the underlying container SHALL reflect the change

### Requirement: Member functions are callable

The binding layer SHALL expose reflected member functions as Python methods that validate arguments before
dispatching.

#### Scenario: Successful call

- **WHEN** a script calls a reflected member function with matching arguments
- **THEN** it SHALL receive the converted return value

#### Scenario: Argument mismatch

- **WHEN** a script calls a reflected member function with the wrong number or types of arguments
- **THEN** the call SHALL raise a Python exception and SHALL NOT dispatch

### Requirement: Values convert between Python and C++

The binding layer SHALL convert values between Python and C++ for scalars, strings, reflected types, and
reflected sequences.

#### Scenario: Scalar and string round-trip

- **WHEN** a script writes a numeric or string value to a member and reads it back
- **THEN** the value SHALL round-trip unchanged

#### Scenario: Nested reflected value

- **WHEN** a script reads a member whose type is itself registered
- **THEN** it SHALL receive a wrapped instance whose members are accessible
