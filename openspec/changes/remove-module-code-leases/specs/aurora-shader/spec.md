## ADDED Requirements

### Requirement: Compilation retains its source snapshot

Accepted compilation SHALL retain a stable owned source/dependency view, inventory and required provider/compiler/data ownership until completion or collected cancellation; defining DLL code SHALL remain process resident. Resource registration/retirement SHALL NOT mutate or destroy backing still used by a compile job. Failures SHALL release borrows through the same lifetime protocol.

#### Scenario: Source owner retires during compilation
- **WHEN** an owner stops accepting requests while a shader job is in flight
- **THEN** the job finishes against its retained snapshot or reports collected cancellation, and ownership is released only after that use ends
