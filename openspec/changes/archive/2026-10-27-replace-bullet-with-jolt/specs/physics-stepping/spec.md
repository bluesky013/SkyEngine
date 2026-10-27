## ADDED Requirements

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
