## ADDED Requirements

### Requirement: Reflected field backing storage lifetime
ReflectedForm SHALL construct descriptor-backed fields at stable final addresses for the lifetime of one built tree. Member presentation order SHALL be resolved before constructing fields; known sibling/element capacity SHALL be reserved before recursively binding owned values and descendants. A child descriptor pointing into a struct field's owned Any and each child parent pointer SHALL remain valid after the complete form is built, independent of inline versus heap Any storage. Default capture SHALL obey the same lifetime rule. Rebuild SHALL own structural replacement; view consumers MAY change presentation/expansion state but SHALL NOT relocate or structurally mutate built fields.

#### Scenario: Inline nested structs remain readable
- **WHEN** a reflected struct smaller than Any's inline block contains another reflected inline struct
- **THEN** all descendant descriptors SHALL read from the final owning field values after construction returns

#### Scenario: Sibling order does not relocate built values
- **WHEN** multiple top-level and nested siblings have presentation order different from reflected member iteration order
- **THEN** the completed tree SHALL expose that presentation order without moving already bound field backing or invalidating child descriptors

#### Scenario: Parent links propagate a deep edit
- **WHEN** a leaf inside multiple inline struct levels is edited or reset
- **THEN** its final parent chain SHALL propagate the new value to the correct root member as one undoable command and SHALL preserve unrelated siblings

#### Scenario: Nested sequence fields retain live containing storage
- **WHEN** a sequence inside multiple reflected struct levels exposes many indexed children
- **THEN** all children SHALL resolve the expected copied element values through live containing storage and preserve their existing read-only element contract

#### Scenario: Scratch baseline fields do not escape their storage
- **WHEN** a form captures type defaults or an explicit baseline using a temporary scratch form
- **THEN** copied default values SHALL remain usable by the live tree without retaining scratch descriptors or addresses
