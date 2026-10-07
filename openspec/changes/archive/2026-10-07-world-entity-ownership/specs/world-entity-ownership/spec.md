## ADDED Requirements

### Requirement: World exclusively owns actors

A world SHALL be the sole owner of its actors. No public API SHALL hand out shared ownership of an actor, and an actor SHALL be destroyed when it leaves its world.

#### Scenario: No shared ownership escapes

- **WHEN** an actor is created or looked up
- **THEN** the returned value SHALL be non-owning, and destroying it SHALL NOT destroy the actor

#### Scenario: Detach destroys

- **WHEN** an actor is detached from its world and no other world owns it
- **THEN** the actor SHALL be destroyed

### Requirement: Non-owning references and explicit ownership transfer

Public references to actors SHALL be non-owning, and ownership SHALL move explicitly: attaching takes ownership of an actor, and detaching returns ownership to the caller.

#### Scenario: Detach returns ownership

- **WHEN** an actor is detached from its world
- **THEN** ownership SHALL be returned to the caller, and the actor SHALL remain alive until the caller releases it

#### Scenario: Reattach after detach

- **WHEN** a detached actor (whose ownership the caller holds) is attached to a world
- **THEN** it SHALL be attached without creating a second owner

#### Scenario: Lookups do not extend lifetime

- **WHEN** an actor is looked up
- **THEN** the returned reference SHALL be non-owning

<!-- Generation-safe handles are not planned; public actor references are non-owning. -->


### Requirement: O(1) lookup and removal

Actor lookup by identity and removal SHALL not scan the actor list.

#### Scenario: Lookup is constant time

- **WHEN** an actor is looked up by its identity in a world with many actors
- **THEN** the lookup SHALL use the index, not a linear scan

### Requirement: Dense storage preserved

Actor storage SHALL remain a packed array so iteration stays cache-friendly.

#### Scenario: Removal keeps storage packed

- **WHEN** an actor is detached
- **THEN** the actor array SHALL remain contiguous with no holes
