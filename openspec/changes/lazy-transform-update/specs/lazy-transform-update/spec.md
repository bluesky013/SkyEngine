## ADDED Requirements

### Requirement: World transform resolves lazily

A transform's world transform SHALL be computed on demand (when read) rather than eagerly on every write. Reading a world transform SHALL return a value consistent with the current local transform and parent chain.

#### Scenario: Read returns current world transform

- **WHEN** a local transform is set and the world transform is then read
- **THEN** the reader SHALL return the value derived from the new local transform and the parent chain

### Requirement: Writes coalesce

Multiple transform writes before a read SHALL NOT recompute the descendant subtree each time; computation SHALL happen at most once per read (or resolve point).

#### Scenario: Repeated writes recompute once

- **WHEN** a transform is written N times and then read once
- **THEN** the world transform SHALL be computed once and equal the last write's result

#### Scenario: Descendants marked, not computed, on write

- **WHEN** an ancestor transform is written
- **THEN** descendants SHALL be marked for update, and their world transforms SHALL be computed only when read

### Requirement: Change events remain correct

A transform-change event SHALL be delivered with a world transform consistent with the current state at delivery.

#### Scenario: Emitter event carries current global

- **WHEN** a transform changes and its change event fires
- **THEN** the world transform carried or queried by the handler SHALL reflect the change

### Requirement: Non-uniform scale policy is defined

The engine SHALL define the behavior for non-uniform scale inherited through rotated ancestors, either by a documented restriction with a diagnostic or by a correct parent-space composition.

#### Scenario: Unsupported scale combination is not silent

- **WHEN** a rotated node inherits non-uniform scale from an ancestor
- **THEN** the engine SHALL either produce a correct result or emit a diagnostic, rather than silently returning an incorrect transform
