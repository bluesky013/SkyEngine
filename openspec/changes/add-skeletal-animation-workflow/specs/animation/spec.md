## ADDED Requirements

### Requirement: Independent mutable evaluation state
Mutable node bindings, players, pose pools and frame state SHALL be isolated per animation instance. An AnimationPlan containing mutable evaluation storage SHALL NOT be evaluated as a shared multi-instance program. Stable instances SHALL reuse their preallocated evaluation storage.

#### Scenario: Concurrent independent evaluations
- **WHEN** separate instances evaluate the same immutable graph definition at different times
- **THEN** they SHALL use disjoint mutable workspaces and produce the same results as separate serial evaluations

### Requirement: Invalid rig and clip input reports failure
The touched asset-to-animation creation path SHALL validate bone counts, unique identities, parent bounds/cycles, reference-pose sizes, finite transforms, key array shapes and times before constructing runtime objects. Recoverable invalid data SHALL return explicit failure and SHALL NOT be handled by assertions, exceptions or silent channel omission.

#### Scenario: Malformed rig or clip
- **WHEN** an asset has a parent cycle, a missing reference transform, a mismatched key array or an unknown bone channel
- **THEN** binding SHALL fail with an asset-specific diagnostic before pose evaluation accesses the data
