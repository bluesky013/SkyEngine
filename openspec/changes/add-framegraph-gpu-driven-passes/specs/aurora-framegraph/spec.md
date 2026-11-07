## ADDED Requirements

### Requirement: Feature-visible external buffer import

The frame graph SHALL allow a render feature to import a caller-owned `BufferPtr` with declared initial and final states plus an initialization flag, and SHALL retain that buffer until the submission using it completes.

#### Scenario: Import a caller-owned persistent buffer

- **WHEN** a feature imports a persistent `BufferPtr` with valid states and initialization, then releases its own CPU reference after building the graph
- **THEN** the graph SHALL retain the buffer until GPU completion, and the feature MAY create or import it again on a later frame

#### Scenario: Duplicate physical import

- **WHEN** two features import the same physical buffer with identical size, states and initialization
- **THEN** the graph SHALL return the same logical buffer handle without consuming an additional resource budget

#### Scenario: Conflicting import

- **WHEN** a buffer is imported with a conflicting size, state or initialization declaration
- **THEN** the graph SHALL reject the later declaration without modifying the earlier one

#### Scenario: Uninitialized persistent read

- **WHEN** a pass reads an imported buffer that is declared uninitialized
- **THEN** Compile SHALL reject the graph before native submission

#### Scenario: In-flight retention

- **WHEN** a feature drops its reference while frames that imported the buffer are still in flight
- **THEN** the graph SHALL keep the buffer alive until those frames complete

### Requirement: Indirect raster draw declaration

The frame graph SHALL support an optional indirect raster draw that reads draw arguments from an initialized graph buffer, mutually exclusive with explicit draw counts.

#### Scenario: Indexed indirect draw

- **WHEN** a raster draw declares an indirect argument buffer with indexed usage, offset and stride
- **THEN** the backend SHALL issue an indexed indirect draw using those arguments

#### Scenario: Explicit and indirect draw mixed

- **WHEN** a raster draw declares both explicit draw counts and indirect arguments
- **THEN** Compile SHALL reject the declaration

#### Scenario: Uninitialized argument buffer

- **WHEN** an indirect draw reads an argument buffer that has no initialized producer
- **THEN** Compile SHALL reject the draw before encoding

#### Scenario: Unsupported device feature

- **WHEN** the selected device does not support the required indirect or multi-draw feature
- **THEN** Compile SHALL return an explicit Unsupported error rather than silently degrading

#### Scenario: Multi-draw gating

- **WHEN** an indirect raster draw declares more than one draw and the device does not support multi-draw indirect
- **THEN** Compile SHALL reject the declaration

### Requirement: Indirect compute dispatch declaration

The frame graph SHALL support an indirect compute dispatch that reads group counts from an initialized graph buffer. Direct and indirect commands SHALL be mutually exclusive alternatives in the compute pass declaration.

#### Scenario: Indirect dispatch

- **WHEN** a compute pass declares indirect arguments with a valid offset
- **THEN** the backend SHALL issue an indirect dispatch using the argument buffer contents

#### Scenario: Exclusive dispatch declaration

- **WHEN** a compute pass selects its dispatch command
- **THEN** it SHALL contain either explicit group counts or indirect arguments, without an additional inactive count field

#### Scenario: Dispatch argument producer

- **WHEN** an earlier compute pass writes the indirect argument buffer
- **THEN** the graph SHALL order the write before the dispatch and synchronize the argument buffer for indirect read

### Requirement: Indirect argument lowering and synchronization

Each supported backend SHALL lower indirect raster and compute declarations into its native indirect command while preserving graph synchronization.

#### Scenario: Vulkan lowering

- **WHEN** a Vulkan graph contains an indirect declaration
- **THEN** the Vulkan compiler SHALL emit the matching `vkCmdDraw*Indirect` or `vkCmdDispatchIndirect` after transitioning the argument buffer to the indirect-read stage

#### Scenario: DX12 lowering

- **WHEN** a DX12 graph contains an indirect declaration
- **THEN** the DX12 compiler SHALL use a matching command signature and `ExecuteIndirect` on the direct command list

#### Scenario: Metal lowering

- **WHEN** a Metal graph contains an indirect declaration
- **THEN** the Metal compiler SHALL use the matching indirect draw or dispatch call, treating zero stride as tightly packed arguments

#### Scenario: Unsupported backend path

- **WHEN** a backend cannot encode the requested indirect shape or lacks the required feature
- **THEN** Compile SHALL return Unsupported and SHALL NOT produce a successful submission receipt


#### Scenario: Logical-device feature agreement

- **WHEN** Vulkan reports an indirect draw capability
- **THEN** the feature SHALL be enabled on the logical device, and native draw counts SHALL remain within its advertised limit

#### Scenario: Indirect stride alignment

- **WHEN** an indirect draw uses a nonzero stride that is smaller than its native command or not four-byte aligned
- **THEN** declaration validation SHALL return an explicit error before backend encoding

#### Scenario: Command-signature cache eviction

- **WHEN** DX12 evicts an indirect command signature from its bounded cache while an encoded graph or command buffer still uses it
- **THEN** that borrower SHALL retain the signature through GPU completion and release it at its normal retirement boundary
