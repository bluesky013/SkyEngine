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

