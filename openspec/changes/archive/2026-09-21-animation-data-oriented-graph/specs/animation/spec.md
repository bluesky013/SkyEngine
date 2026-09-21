## ADDED Requirements

### Requirement: Data-oriented evaluation plan
The animation runtime SHALL evaluate a flattened, topologically ordered array of fixed-size operation records over a preallocated pose pool instead of dispatching virtual node calls in the data plane. Each operation record SHALL identify its operation kind, input and output pose slots, a weight, and a data index.

#### Scenario: Operations evaluate in order
- **WHEN** a compiled plan is evaluated
- **THEN** each operation SHALL read its declared input slots and write its declared output slot in topological order

#### Scenario: Layer weight is data
- **WHEN** a node evaluates with a layer weight
- **THEN** the weight SHALL be carried by an operation record field rather than a recursively propagated context

### Requirement: Control plane and data plane separation
The animation runtime SHALL separate a control plane that performs state-machine transitions, condition evaluation, parameter updates, clip time advance, root-motion decisions, and event dispatch from a data plane that performs only clip sampling, blending, and skin-matrix generation. The data plane SHALL consume a frame-state block produced by the control plane and SHALL NOT perform data-dependent branching on graph structure.

#### Scenario: State machine stays on the control plane
- **WHEN** a state transition occurs
- **THEN** the transition SHALL be decided on the control plane and the data plane SHALL only receive the resulting frame state

#### Scenario: Data plane is pure
- **WHEN** the data plane evaluates the same frame state twice
- **THEN** it SHALL produce the same pose without requiring additional control-plane input

### Requirement: Allocation-free evaluation
Evaluating a compiled plan SHALL NOT perform heap allocation. Pose slots, blend scratch, and transition search storage SHALL be owned by the plan or its instance and reused across frames.

#### Scenario: Repeated evaluation does not allocate
- **WHEN** a plan is evaluated across many frames
- **THEN** no heap allocation SHALL occur during evaluation

### Requirement: Index-based bone and channel resolution
Channel-to-bone and bone-name resolution SHALL be performed when the plan is compiled, producing index ranges used by the evaluation loop. The evaluation loop SHALL NOT perform name-based or hash-based lookups.

#### Scenario: No hashing during sampling
- **WHEN** a compiled clip is sampled
- **THEN** channel and bone access SHALL use precomputed indices only

### Requirement: Structure-of-arrays keyframe layout
Clip keyframes SHALL be stored as separate fixed-stride time and value blocks per channel component so the data can be read coalesced and uploaded without repacking.

#### Scenario: Keyframe blocks are separated
- **WHEN** a channel component's keyframes are stored
- **THEN** its times and values SHALL occupy separate contiguous blocks with a fixed element stride

### Requirement: Pointer-free serializable graph data
The compiled plan together with its clip, skeleton, and bone-mask data SHALL form a self-contained, versioned byte layout containing no pointers and using offsets for all cross-references. The CPU evaluator SHALL read this layout directly.

#### Scenario: No pointers in the compiled layout
- **WHEN** a compiled plan is serialized
- **THEN** it SHALL contain offsets and indices only, with no host pointers

#### Scenario: Layout round trip
- **WHEN** a compiled plan is serialized and deserialized
- **THEN** evaluating it SHALL produce the same pose as the in-memory plan

### Requirement: Graph compilation and validation
The authoring graph SHALL be compiled into the evaluation plan, and compilation SHALL validate the graph and SHALL reject invalid graphs with a diagnostic instead of evaluating them. Validation SHALL detect cycles, out-of-range references, a missing or multiple output operation, and bone indices outside the skeleton.

#### Scenario: Cyclic graph rejected
- **WHEN** the authoring graph contains a cycle
- **THEN** compilation SHALL fail with a diagnostic and no plan SHALL be produced

#### Scenario: Out-of-range reference rejected
- **WHEN** the graph references a state, condition, or bone outside its declared range
- **THEN** compilation SHALL fail with a diagnostic

### Requirement: Authoring API preserved
The existing node types SHALL remain available as the authoring and graph-building surface, and SHALL lower to the evaluation plan. Authoring code SHALL NOT be required to construct operation records directly.

#### Scenario: Existing authoring builds a plan
- **WHEN** a graph is built through the existing node and state-machine authoring API
- **THEN** it SHALL compile to an evaluation plan without direct operation-record construction

### Requirement: Plan supports bone masking
A compiled clip operation SHALL reference a bone mask, and the evaluator SHALL NOT apply channels whose bone is excluded by the mask. A clip node's bone mask SHALL be captured when it is lowered.

#### Scenario: Masked bone keeps the bind pose
- **WHEN** a clip node with a cleared bone bit is compiled and evaluated
- **THEN** the excluded bone SHALL keep the bind pose and the remaining bones SHALL be sampled

#### Scenario: Unmasked clip samples normally
- **WHEN** a clip node with a full bone mask is compiled and evaluated
- **THEN** all of its tracks SHALL be applied

### Requirement: Plan exposes the consumed root-motion delta
Evaluating a plan SHALL be able to return the consumed root-motion delta, and SHALL NOT mutate shared pose state such as the bone mask.

#### Scenario: Root-motion delta output
- **WHEN** a plan with root motion disabled samples a moving root and is evaluated with a delta output
- **THEN** the root bone SHALL be reset to the bind pose and the delta output SHALL hold the sampled root offset

### Requirement: Plan blend weights are runtime-driven
Blend operations SHALL read their weight from the frame state when a weight slot is assigned, so blend weights can change between frames without recompiling. Multi-pose weights SHALL be normalized at runtime.

#### Scenario: Two-pose weight follows the frame state
- **WHEN** the frame state weight of a two-pose blend changes from 0 to 1
- **THEN** the evaluated pose SHALL follow from the first pose to the second

#### Scenario: List weights are normalized
- **WHEN** a pose list's runtime weights change
- **THEN** the output SHALL be the weighted average of the active poses at their current weights

### Requirement: Plan supports additive blending
A two-pose node marked additive SHALL lower to an additive blend operation that accumulates a weighted delta onto the base pose.

#### Scenario: Additive blend op emitted
- **WHEN** a two-pose node is marked additive and compiled
- **THEN** the plan SHALL contain an additive blend operation and evaluate the base pose plus the weighted delta

### Requirement: Layer weight semantics
A layer operation SHALL blend its input pose over the plan's bind pose by its weight, so a layer at weight 0 keeps the bind pose and at weight 1 equals its input.

#### Scenario: Half-weight layer
- **WHEN** a layer operation with weight 0.5 is evaluated against a bind pose and an input pose
- **THEN** the output SHALL be the midpoint between the bind pose and the input pose

### Requirement: Node lowering extension point
Authoring node types SHALL lower themselves into the plan through a virtual lowering entry point. Nodes that do not implement it SHALL be rejected by the compiler with a diagnostic rather than silently skipped.

#### Scenario: Unsupported node rejected
- **WHEN** a node that does not implement lowering is compiled
- **THEN** compilation SHALL fail with a diagnostic and produce no plan

#### Scenario: Nested nodes lower through the entry point
- **WHEN** a composite node lowers its children
- **THEN** each child SHALL lower through the same entry point without the compiler inspecting concrete node types

### Requirement: Inactive state operations are skipped
When a state machine is compiled, the control plane SHALL mark only the active state's operations as enabled, and the data plane SHALL skip disabled operations.

#### Scenario: Only the active state is evaluated
- **WHEN** a state machine plan is ticked with one active state
- **THEN** the operations of the inactive states SHALL be disabled while the active state's operations, state selection, and output remain enabled

### Requirement: Comparative performance benchmark
The animation tests SHALL include a benchmark that evaluates the same graph through both the virtual and the flat evaluators and compares heap allocations and evaluation time. The flat evaluator SHALL perform zero heap allocations during evaluation and SHALL NOT regress beyond a wide time margin against the virtual evaluator.

#### Scenario: Allocation benchmark
- **WHEN** an equivalent blend graph is evaluated many times through both paths
- **THEN** the virtual path SHALL allocate during evaluation and the flat path SHALL allocate zero times

#### Scenario: Time benchmark
- **WHEN** per-evaluation time is measured for both paths on the same graph
- **THEN** the flat path SHALL stay within a wide margin of the virtual path and the result SHALL be reported


