## ADDED Requirements

### Requirement: Mesh material policy is an explicit compilation input
Generic mesh binding and compilation SHALL use explicit material identities supplied by the caller and SHALL NOT select or register a global source-path fallback. Builders SHALL resolve their fallback policy through existing cook settings and Engine defaults at the builder boundary.

#### Scenario: Empty material slots
- **WHEN** a builder compiles geometry without material slots
- **THEN** its explicit fallback is bound and included in the result dependencies
- **AND** when no fallback UUID is configured and the Engine default source is unavailable, preparation returns an explicit failure

#### Scenario: Explicit material reference
- **WHEN** the caller supplies material UUIDs or configures a nonzero default material UUID
- **THEN** compilation preserves those identities and reports them as effective dependencies
- **AND** dependency/resource loading retains responsibility for asset existence and material type validation

### Requirement: Mesh authoring returns effective dependencies
Mesh authoring compilation SHALL report the effective material dependency set. SoftBody builders SHALL propagate that set to source dependencies, including implicit defaults selected by builder policy.

#### Scenario: SoftBody generated mesh uses fallback
- **WHEN** a SoftBody recipe has no explicit material slots and compilation selects a fallback
- **THEN** both the generated mesh product and source dependency graph reference the effective material
