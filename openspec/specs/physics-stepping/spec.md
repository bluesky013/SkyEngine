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

### Requirement: Step completion reports native failure

World Step SHALL return an engine PhysicsStatus. Native capacity/update errors SHALL be translated into that status after all scheduled work is collected. A failed native update SHALL put the world in a faulted state, prevent further stepping and expose the failure to its host; it SHALL NOT publish the partial update as a successful simulation frame or claim that it was rolled back.

#### Scenario: Native contact capacity is exceeded
- **WHEN** a native update reports contact or pair capacity exhaustion
- **THEN** Step SHALL complete all jobs, return CapacityExceeded, fault the world and withhold normal successful-frame notification

#### Scenario: Caller steps a faulted world
- **WHEN** another Step is requested before explicit world reconstruction
- **THEN** it SHALL return InvalidState without further native simulation

#### Scenario: Invalid timestep
- **WHEN** the fixed timestep is zero, negative or non-finite
- **THEN** Step SHALL reject it before native mutation

### Requirement: Tick and single-step share completion behavior

PhysicsSystem Tick and SingleStep SHALL use the same fixed-step completion path, including status handling, character input consumption, pose synchronization, event dispatch and interpolation updates. Paused time SHALL NOT accumulate hidden simulation steps.

#### Scenario: Editor single-step
- **WHEN** a paused world receives SingleStep
- **THEN** it SHALL execute one fixed step and publish its resulting poses and events using the same completion rules as Tick

#### Scenario: Character command spans a render frame
- **WHEN** a displacement is queued and a render frame causes multiple fixed steps
- **THEN** the queued displacement SHALL be consumed once rather than applied in full on every substep

### Requirement: Worker callbacks do not directly invoke gameplay

Native contact callbacks SHALL only update world-owned synchronized contact data. After all jobs finish, the host thread SHALL reduce subshape contacts to object pairs and emit events ordered by stable engine handles. Sleeping persistent contacts SHALL NOT produce false end events.

#### Scenario: Compound body has several touching children
- **WHEN** several native subshape pairs represent the same object contact
- **THEN** the host SHALL emit one logical begin/stay/end sequence for that object pair

#### Scenario: Contact persists while both objects sleep
- **WHEN** native contact callbacks are not repeated for sleeping bodies
- **THEN** the engine SHALL preserve the persistent contact until removal or actual separation

### Requirement: Scene input precedes simulation and dynamic output follows it

Actor transform edits SHALL update static/dynamic body poses or submit kinematic targets. World Tick SHALL run actor input, subsystem simulation and then component PostTick. Physics output SHALL update only dynamic actor poses and SHALL prevent transform feedback into the backend.

#### Scenario: Kinematic actor moves
- **WHEN** an actor supplies a new kinematic transform
- **THEN** the next physics step SHALL move toward that target, and stale interpolation SHALL NOT overwrite the actor's authored transform

#### Scenario: Dynamic interpolation is published
- **WHEN** World finishes a physics tick
- **THEN** the actor SHALL receive the interpolation result from that completed tick rather than the preceding host tick

### Requirement: Adaptor event batches have a bounded host-frame lifetime

PhysicsSystem SHALL dispatch all ordered events from the completed host Tick/SingleStep to listeners. Its polling batch SHALL be consumable until the next host tick, after which it SHALL be retired; it SHALL NOT accumulate an unbounded history when nobody polls. Listener Begin/End delivery SHALL NOT depend on polling.

#### Scenario: Ordinary host never drains events
- **WHEN** persistent contacts run for many host ticks without a polling consumer
- **THEN** retained adaptor events SHALL be bounded by the current host batch rather than simulation duration

#### Scenario: Multiple listeners observe a trigger
- **WHEN** a body enters and leaves a trigger across host steps
- **THEN** each registered listener SHALL see the ordered Enter/Exit sequence, while polling SHALL expose only the current batch

