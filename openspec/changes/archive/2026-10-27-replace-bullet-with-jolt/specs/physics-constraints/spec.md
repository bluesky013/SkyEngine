## MODIFIED Requirements

### Requirement: Backend-neutral constraint descriptors

The physics module SHALL describe fixed, hinge, slider, distance and generic 6-DOF constraints as plain data with body handles, body-origin-local anchors/axes, limits and motors. An explicitly invalid handle SHALL denote a world anchor; a supplied stale or wrong-kind handle SHALL be rejected. Motor data SHALL specify Velocity or Position mode, defaulting to Velocity for existing data.

#### Scenario: Describe a joint without a backend
- **WHEN** a constraint descriptor is created
- **THEN** it SHALL reference engine data and body handles only and SHALL NOT include backend types

#### Scenario: Single-body and two-body constraints
- **WHEN** a constraint references one live body and an explicit world anchor, or two live bodies
- **THEN** the backend SHALL implement the corresponding single-body or two-body constraint

#### Scenario: Supplied body is stale
- **WHEN** a supplied non-invalid body handle cannot be resolved
- **THEN** creation SHALL fail without treating that handle as a world anchor

### Requirement: Limits and drives are describeable

Constraint descriptors SHALL express angular/linear limits and explicit motor modes. The backend SHALL apply supported fields and reject nondefault unsupported fields before creating any constraint. Hinge SHALL use angularX and the angular motor; Slider SHALL use linearX and the linear motor; Generic6DOF SHALL apply all six limits and bind the existing scalar motors to their local X axes. A Generic6DOF axis with limited=false SHALL be free; equal lower/upper bounds SHALL lock it.

#### Scenario: Configure limits
- **WHEN** a hinge is described with angularX limits
- **THEN** the backend SHALL enforce those limits in simulation

#### Scenario: Choose motor mode
- **WHEN** a supported motor selects Position mode
- **THEN** its position target and force/torque limit SHALL be applied rather than being interpreted as a velocity drive

#### Scenario: Unsupported drive is requested
- **WHEN** a fixed or distance constraint carries an unsupported enabled motor
- **THEN** creation SHALL return an explicit unsupported result and SHALL NOT publish a partially working constraint

## ADDED Requirements

### Requirement: Distance constraints enforce authored length

A distance constraint SHALL maintain its authored nonnegative anchor distance. A zero distance SHALL use an equivalent point constraint. It SHALL NOT silently implement every distance as coincident anchors.

#### Scenario: Nonzero distance
- **WHEN** two bodies are connected with a nonzero distance
- **THEN** simulation SHALL preserve that anchor separation within solver tolerance

### Requirement: Connected collision exclusion is pair-specific

When collideConnected is false, the backend SHALL exclude only the referenced body pair from mutual collision. Removing that constraint SHALL restore the pair's normal filter behavior unless another live constraint still excludes it.

#### Scenario: Three-body chain
- **WHEN** A-B and B-C constraints exclude their connected pairs
- **THEN** A-C collision SHALL remain governed by its ordinary masks
