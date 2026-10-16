## ADDED Requirements

### Requirement: Exception translation is limited to provider boundaries
Preferences SHALL translate failures only at injected guard, reflection/provider and storage-adapter calls covered by its structured result protocol. Ordinary JSON/collection/allocation work SHALL NOT acquire whole-operation catches. Session SHALL reuse shared candidate capture. Any and file resources SHALL use RAII or equivalent ownership that releases incomplete resources when construction or allocation exits exceptionally.

#### Scenario: Reflected copy throws before completing construction
- **WHEN** a reflected value constructor throws
- **THEN** unconstructed allocation SHALL be released without running the complete-object destructor, and capture SHALL retain its existing structured failure result

#### Scenario: Ordinary file or memory failure occurs
- **WHEN** file APIs report an IO error
- **THEN** storage SHALL report the existing IO result and preserve failed-write destination bytes; unrelated allocation exceptions SHALL propagate through RAII cleanup without a new blanket catch
