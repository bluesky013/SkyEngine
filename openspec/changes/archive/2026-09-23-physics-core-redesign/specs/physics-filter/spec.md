## ADDED Requirements

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
