## ADDED Requirements

### Requirement: Runtime preserves the ECS preparation boundary

The renderer runtime SHALL use the existing CPU coordinator, frozen ECS system program and prepared-product collection as the only scene preparation path. Derived component writes and view extraction SHALL retain declared accesses and producer dependencies. GPU graph stages SHALL consume owned prepared products without accessing mutable ECS storage.

#### Scenario: Scene data is prepared before view extraction
- **WHEN** an accepted packet changes an entity or its selected resource version
- **THEN** the scene producer updates invalidated derived data before dependent view extraction
- **AND** the runtime queues GPU work only from successfully collected prepared products

#### Scenario: A reader outlives prepare kernel execution
- **WHEN** a frozen read view remains borrowed after kernels finish
- **THEN** the coordinator waits for the borrower before the runtime's prepared callback, the next packet mutation or teardown

### Requirement: Packet synchronization precedes ECS freezing

The runtime SHALL synchronize resource subscriptions and output metadata on the coordinator thread before ECS freezing, including bindings changed while their outputs are suspended.

#### Scenario: A moving mesh changes with a suspended viewport
- **WHEN** mesh binding revisions continue to change while output extent is zero
- **THEN** the current mesh and material subscribers remain synchronized
- **AND** rendering resumes with the current bindings when the output becomes available

### Requirement: GPU orchestration preserves cleanup and completion order

The runtime SHALL expose explicit graph and progress stages while preserving uploader-owned completion polling, output acquisition, graph lifetime retention and retirement ordering. Stage failures SHALL propagate explicitly to cleanup that releases graph borrowers before abandoning the output or frame slot.

#### Scenario: Graph construction fails after output acquisition
- **WHEN** a graph stage reports an error
- **THEN** acquired output borrowers and builder references are released before the frame is discarded
- **AND** the accepted packet receives its existing failure notification

#### Scenario: Upload preparation admits no work
- **WHEN** upload preparation returns an invalid batch ID because no upload work was admitted
- **THEN** the runtime does not attempt to discard a nonexistent batch
- **AND** the frame can still render or complete through its normal no-work path

#### Scenario: CPU preparation hands off a successful frame
- **WHEN** a prepared frame is queued for GPU processing
- **THEN** the GPU path owns its final completion notification
- **AND** CPU acknowledgement does not emit a duplicate frame completion
