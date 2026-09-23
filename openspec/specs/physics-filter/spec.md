# physics-filter Specification

## Purpose
TBD - created by archiving change physics-backend-abstraction. Update Purpose after archive.
## Requirements
### Requirement: Documented backend-neutral filter contract

The physics module SHALL define and document a backend-neutral collision filtering contract over the `CollisionFilterBit` group/mask model, and each backend SHALL map it consistently.

#### Scenario: Filter contract is defined

- **WHEN** the physics filtering contract is inspected
- **THEN** it SHALL specify how group and mask are interpreted and how a backend must map them

#### Scenario: Objects respect group and mask

- **WHEN** two objects are tested for collision with group/mask values that do not permit interaction
- **THEN** they SHALL NOT interact under the active backend

### Requirement: Bullet backend filter mapping

The Bullet backend SHALL map the group/mask contract onto Bullet's collision filtering so the contract holds for the current backend.

#### Scenario: Bullet honors group and mask

- **WHEN** Bullet-backed objects are added with group and mask values
- **THEN** collisions SHALL be filtered according to the documented contract

### Requirement: Filter contract supports a filter-shader backend

The filter contract SHALL be expressible by a backend that uses a filter function (for example a PhysX filter shader) without changing the engine-side contract.

#### Scenario: Filter is expressible as a function

- **WHEN** the documented contract is implemented as a boolean filter function of two objects' group and mask
- **THEN** it SHALL produce the same accept/reject decision as the Bullet mapping

### Requirement: Named interaction layers

The physics module SHALL allow collision filter bits to be associated with named interaction layers registered at engine level, so authors express filtering by layer name rather than raw bits.

#### Scenario: Resolve a layer by name

- **WHEN** a body is authored with a named layer and a named mask set
- **THEN** the module SHALL resolve them to group/mask bits consistently across runs

#### Scenario: Unknown layer is rejected

- **WHEN** a body references a layer name that is not registered
- **THEN** the module SHALL reject it or fall back to a documented default rather than silently using arbitrary bits

### Requirement: Query filter contract

A query filter SHALL combine a mask, an optional set of ignored handles, and an optional layer matrix, and SHALL be applied by every query kind (raycast, sweep, overlap) with consistent semantics.

#### Scenario: Ignore a handle

- **WHEN** a query carries the handle of the object issuing it as ignored
- **THEN** that object SHALL be excluded from the query results

#### Scenario: Consistent across query kinds

- **WHEN** the same filter is applied to a raycast, a sweep, and an overlap
- **THEN** the accept/reject decision for a given object SHALL be the same across all three

### Requirement: Backend mapping of the query filter

Each backend SHALL map the query filter contract onto its filtering mechanism so that query results match the documented contract.

#### Scenario: Bullet maps the query filter

- **WHEN** a filtered query is executed on the Bullet backend
- **THEN** the results SHALL honor the mask, ignored handles, and layer matrix per the contract

