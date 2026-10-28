## ADDED Requirements

### Requirement: Compile-local resource-use facts
Compilation SHALL derive effective buffer byte ranges, normalized image subresource ranges, access modes and resource usage roles from the same typed declaration traversal used for graph analysis. Reads, writes and content definitions MUST remain distinct. These facts SHALL be temporary compilation data, not a second persistent frozen IR.

#### Scenario: GPU-produced raster storage
- **WHEN** a copy or compute pass writes a storage buffer consumed by a later raster pass
- **THEN** analysis retains its producer and each native compiler synchronizes the storage read, combining other simultaneous read roles on the same physical buffer

#### Scenario: Declaration validation before culling
- **WHEN** an otherwise dead raster or compute pass has invalid reflection/bindings
- **THEN** declaration validation fails consistently before culling, while undefined content checks remain limited to surviving work and exports

### Requirement: Immutable logical binding groups
Raster and compute bindings SHALL use immutable logical group snapshots derived from the shader contract. Per-use access and dynamic offsets SHALL remain separate. Groups MUST retain exact resource identities/ranges and support sparse sets; Custom resource order MUST remain unchanged.

#### Scenario: Shared pass and material with distinct batch offsets
- **WHEN** two draws share compatible Global, Pass and Material values and differ in Batch dynamic offsets
- **THEN** shared groups are encoded/uploaded and natively materialized once within the graph/lease, and each draw uses its own effective offset

#### Scenario: Incompatible shader or resource range
- **WHEN** a layout, resource identity, image view or base range differs
- **THEN** incompatible native bindings are not reused, and prior in-flight groups remain valid until safe retirement

### Requirement: Attachment resolve preserves declared source contents
The Metal native raster plan SHALL jointly lower attachment store and resolve semantics in the original render pass. A requested resolve MUST remain defined when the source is discarded, and STORE MUST preserve the source for subsequent legal LOAD operations.

#### Scenario: Resolve and discard
- **WHEN** an MSAA color or depth attachment uses DONT_CARE store and has a resolve target
- **THEN** the resolve target receives the rendered values before the source is discarded

#### Scenario: Resolve and preserve
- **WHEN** an attachment uses STORE and resolve followed by a later LOAD
- **THEN** both the resolved target and the source attachment retain their declared contents

### Requirement: Exact native plan identity and failure receipt
Dispatch SHALL validate the exact issued plan object and its public identity before backend downcasting. Starting encoding SHALL consume the plan once. Any native recording failure MUST prevent publication of a successful complete graph receipt while preserving safe cleanup and collection.

#### Scenario: Foreign or forged plan
- **WHEN** Dispatch receives a plan from another context/backend or an unknown implementation with a copied identity
- **THEN** it returns an explicit error without downcasting or starting native encoding

#### Scenario: Encoding failure and discard
- **WHEN** recording fails after Dispatch accepted a plan
- **THEN** collection reports failure, the caller no longer owns the consumed plan, and safe unsubmitted discard does not require an extra plan reset

### Requirement: Separate parameter and native write protocols
Named/typed immutable parameter updates SHALL use a distinct interface from numeric native descriptor writes and SHALL reuse the existing semantic transaction. The redundant DescriptorBatch public protocol SHALL be removed with its callers migrated.

#### Scenario: Immutable update publication
- **WHEN** a named update completes successfully
- **THEN** it publishes a new immutable ResourceGroup snapshot; failure leaves the previous snapshot unchanged and numeric writes cannot mutate a published snapshot

### Requirement: Explicit direct-frame compatibility interface
DeviceFrameContext SHALL expose the lease protocol independently of a separately-created direct-frame compatibility interface. Fence queries MUST NOT select or change protocol; direct begin/reset errors MUST be explicit.

#### Scenario: Fence query is observational
- **WHEN** a caller queries a direct context fence
- **THEN** the query does not switch protocol or mutate frame progress

### Requirement: Complete feature graph declarations
Feature graph declaration services SHALL expose the existing supported transient buffer/image, copy, raster/fullscreen, compute, Custom and export operations separately from resource/draw resolution services. Submission SHALL remain coordinator-owned.

#### Scenario: GPU-only intermediate buffer
- **WHEN** a feature declares a compute output buffer
- **THEN** it can create transient GPU-only storage without uploading placeholder CPU bytes and can connect its declared producer and consumer within the same graph
