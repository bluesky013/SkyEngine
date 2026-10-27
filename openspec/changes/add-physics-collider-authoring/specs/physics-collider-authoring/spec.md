## ADDED Requirements

Delivery status (2026-10-27): version-1 geometry/material codecs, stable element IDs, runtime references, local rotation, independent builder publication and reflected component fields are implemented. Recipe/dependency fingerprints, full transform validation, generation/import/decomposition workflows and the dedicated authoring editor/preview remain subsequent work R1–R3/R7 in remaining-work.md. The full requirements below must not be interpreted as completed editor/tool delivery.

### Requirement: Reusable collision sources and independent products
The system SHALL store editable collision recipes and stable shape identities in a plugin-owned source asset, and SHALL publish versioned geometry products consumable without render source files. Body kind, mass and filters SHALL remain instance settings.

#### Scenario: Shared geometry with independent bodies
- **WHEN** two bodies reference one Collision asset with different mass and filters
- **THEN** they share immutable geometry while retaining their independent body settings

#### Scenario: Stale product cannot silently ship
- **WHEN** source save succeeds but product cooking fails
- **THEN** the editor preserves the last successful product, marks it stale, and packaging rejects that stale product

### Requirement: Persistent body material references
Physics materials SHALL have plugin-owned versioned source/product data and stable asset UUIDs. Body authoring data SHALL reference materials by UUID, independently of shared collision geometry. The initial asset schema SHALL expose one friction value, restitution and combine rules; damping SHALL be an explicit body setting. Runtime material handles SHALL be resolved and owned per physics world, never serialized into assets or scenes.

#### Scenario: Shared material across two worlds
- **WHEN** two worlds load bodies referencing the same material asset
- **THEN** they may share immutable product values but each resolves its own runtime material handle and unloading one world does not invalidate the other

#### Scenario: Default versus missing material
- **WHEN** a legacy body has no material UUID or a new body explicitly selects the default
- **THEN** it uses the documented default material
- **WHEN** a nonempty material UUID cannot resolve to a compatible product
- **THEN** the body remains unactivated with an explicit error rather than silently using the default

#### Scenario: Package a material-only dependency
- **WHEN** a material is referenced only by a PhysicsBody in a scene cell
- **THEN** the cell dependency list and package audit include its typed UUID even without a rendering reference

### Requirement: Explicit geometry generation and limits
The authoring system SHALL provide box, sphere, capsule, single convex hull, compound and static triangle mesh workflows with visible inputs, effective parameters, validation results and budgets. It MUST reject dynamic or kinematic triangle meshes and triangle-mesh volume triggers under the initial runtime contract.

#### Scenario: Concave dynamic prop
- **WHEN** the user assigns a concave triangle mesh to a dynamic body
- **THEN** validation rejects the assignment and offers primitive/convex composition without silently changing the body kind

#### Scenario: Degenerate fitting input
- **WHEN** fitting receives non-finite points, invalid indices or dimensions below the supported minimum
- **THEN** it returns a localized error and preserves the previous authored shapes

### Requirement: Shape transforms and legacy compatibility
The system SHALL support local shape rotation and translation through nested compounds with a documented transform order. Existing inline serialized shapes without rotation or asset-reference fields SHALL retain identity rotation and Inline source mode.

#### Scenario: Legacy component load
- **WHEN** an existing PhysicsBody document without new fields is opened
- **THEN** its shape and serialized enum meanings remain unchanged

#### Scenario: Invalid asset reference
- **WHEN** Asset mode references a missing or incompatible Collision product
- **THEN** the system returns an explicit error without substituting an inline default shape

### Requirement: Reflected and undoable authoring UI
The editor SHALL use the shared asset-document, reflected-form, command and action registries. A drag or accepted generation result SHALL be one undoable operation, and cancellation SHALL restore the starting state.

#### Scenario: Cancelled shape drag
- **WHEN** the user presses Escape while resizing a capsule
- **THEN** the original parameters and preview are restored without a committed undo entry

#### Scenario: Generated group replacement
- **WHEN** a valid generated group is applied then undone
- **THEN** the previous shapes, identities and selection are restored through the document command service

### Requirement: Generation tasks and reimport are transactional
Generation SHALL use document revisions, bounded work and exactly one completion notification. Reimport SHALL preserve manual shapes and expose conflicts for modified generated elements. Decomposition that exceeds its budget MUST report unmet quality rather than silently truncate output.

#### Scenario: Stale background result
- **WHEN** generation completes after the document is edited, cancelled or closed
- **THEN** the result is discarded without changing the current document and all task resources are released

#### Scenario: Reimport with edited generated shapes
- **WHEN** the source mesh changes after a generated shape was manually edited
- **THEN** the editor reports a merge conflict while retaining that edit and unrelated manual shapes

### Requirement: Physics preview and replacement preserve world ownership
Preview SHALL use an isolated physics world with reset, pause and single-step. Applying new geometry SHALL validate and construct it before replacing live state, preserve or explicitly rebind dependent constraints, and retain the old state on failure.

#### Scenario: Failed live shape replacement
- **WHEN** candidate geometry cannot be cooked or installed
- **THEN** the original body and its constraints remain usable and the failure is reported

### Requirement: Offline dependencies and unique metadata owners
Runtime adaptor and render modules MUST NOT depend on editor or builder targets. Builder MUST NOT depend on runtime adaptor or render implementations. Shared source codecs SHALL perform no reflection registration; each reflected type SHALL have one definition owner and leased code lifetime.

#### Scenario: Runtime-only deployment
- **WHEN** the game is packaged without authoring tools and CoACD
- **THEN** Collision products load and simulate through the Jolt adaptor without those tool dependencies

#### Scenario: Editor module restart with open work
- **WHEN** an authoring module shuts down with an open document or task
- **THEN** tasks are cancelled and collected, registrations are removed symmetrically, and leased callbacks remain valid until released
