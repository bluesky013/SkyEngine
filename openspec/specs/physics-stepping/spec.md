# physics-stepping Specification

## Purpose
TBD - created by archiving change physics-core-redesign. Update Purpose after archive.
## Requirements
### Requirement: Fixed-timestep advancement

Physics simulation SHALL advance on a configurable fixed timestep driven by the engine, and a frame SHALL run zero or more fixed steps according to an accumulator rather than a single variable-delta step.

#### Scenario: Accumulator runs the correct number of steps

- **WHEN** a frame delta is submitted with a configured fixed timestep
- **THEN** the simulation SHALL execute a whole number of fixed steps equal to the accumulated time divided by the fixed timestep

#### Scenario: Spiral of death is bounded

- **WHEN** a frame delta is much larger than the fixed timestep
- **THEN** the number of steps SHALL be capped by a configured maximum and the excess time SHALL be discarded

### Requirement: Deterministic simulation inputs

The engine and backend step paths SHALL NOT read wall-clock time or the rendered frame delta as simulation inputs; simulation SHALL depend only on the fixed timestep, the object state, and commands issued between steps.

#### Scenario: No frame-delta dependence

- **WHEN** the same sequence of commands is applied with different render frame rates
- **THEN** the simulated result SHALL be the same for the same fixed-step count

#### Scenario: Reproducible on the same build

- **WHEN** the same world is simulated twice from the same initial state and input sequence on the same build
- **THEN** the resulting object states SHALL be identical

#### Scenario: Cross-platform reproducibility is reserved

- **WHEN** cross-platform bit-exact reproducibility is required
- **THEN** it SHALL be provided only by the reserved `Exact` mode (`physics-determinism`), not by the delivered `Fast` mode

### Requirement: Stable object ordering during a step

The engine SHALL present objects to the backend in a deterministic order (stable id or insertion order) so that simulation results do not depend on container iteration or hashing.

#### Scenario: Ordering is independent of storage

- **WHEN** a step is executed
- **THEN** object processing order SHALL be deterministic and not vary between runs

### Requirement: Transform interpolation for rendering

The world SHALL expose, per body, both the last simulated transform and an interpolated transform derived from the previous and current fixed steps, so rendering can be decoupled from the fixed step.

#### Scenario: Interpolated transform is provided

- **WHEN** rendering occurs between two fixed steps with a fractional accumulator remainder
- **THEN** the world SHALL provide a blend of the previous and current simulated transforms

### Requirement: Job-friendly step pipeline with fallback

The step SHALL be expressed as ordered phases with a defined read/write contract, and the engine SHALL dispatch them through a job seam when the active backend advertises job support; otherwise the step SHALL run single-threaded. Any phase accumulation or reduction SHALL use a deterministic order so parallel execution yields the same result as single-threaded execution.

#### Scenario: Single-threaded fallback

- **WHEN** the backend does not advertise job support or the job system is unavailable
- **THEN** the step SHALL execute correctly on a single thread

#### Scenario: Phases respect the read/write contract

- **WHEN** phases are dispatched in parallel and a phase declares write access to an object
- **THEN** no other phase SHALL concurrently write the same object

#### Scenario: Parallel result matches single-threaded

- **WHEN** the same step is run with job stepping enabled and disabled
- **THEN** the resulting simulation state SHALL be identical

### Requirement: Step statistics

The world SHALL expose per-step statistics (step count, active body count, and per-phase timing when profiling is enabled) without requiring a render dependency.

#### Scenario: Stats are available after a step

- **WHEN** stats are queried after stepping
- **THEN** the world SHALL report the number of steps executed and the active body count

