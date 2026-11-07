> **Current status - 2026-11-07: review corrections implemented and verified on macOS; remaining work is listed below.** This change is a prerequisite for `add-particle-system`; it does not depend on particle code.

## 1. API surface

- [x] 1.1 Add `CreatePersistentBuffer(const Buffer::Descriptor&)` to `FrameFeaturePrepareContext` and implement it in the coordinator preparation facet
- [x] 1.2 Add `ImportBuffer(BufferPtr, CopyBufferState, CopyBufferState, bool)` to the feature graph declaration facet and forward it to `FrameGraphBuilder::Import`
- [x] 1.3 Add optional `RasterIndirectDraw` to `RasterDraw` and exclusive direct/indirect command alternatives to `FrameComputePass`
- [x] 1.4 Keep indirect fields mutually exclusive with explicit draw/dispatch counts and expose explicit errors

## 2. Validation and analysis

- [x] 2.1 Validate imported buffer size, states, initialization, device ownership and duplicate physical identity
- [x] 2.2 Validate indirect ranges, strides, draw counts, indexed shape and initialized argument producers
- [x] 2.3 Gate indirect declarations on `multiDrawIndirect`, `firstInstanceIndirect` and backend-equivalent dispatch support
- [x] 2.4 Extend resource-use collection, content dependencies and lifetime analysis for imported buffers and indirect arguments
- [x] 2.5 Add negative tests for mixed explicit/indirect, uninitialized arguments, invalid ranges and unsupported features

## 3. Native lowering

- [x] 3.1 Implement Vulkan indirect draw/dispatch lowering and argument-buffer synchronization
- [x] 3.2 Implement DX12 command-signature caching and `ExecuteIndirect` lowering
- [x] 3.3 Implement Metal indirect draw/dispatch lowering with zero-stride tight packing
- [x] 3.4 Ensure unsupported shapes return `Unsupported` and never produce a successful receipt

## 4. Tests and verification

- [x] 4.1 Add import lifetime tests: caller drops reference, duplicate imports, conflicting imports and in-flight retention
- [ ] 4.2 Add GPU readback tests for compute-written indirect arguments consumed by raster/compute
- [ ] 4.3 Add backend encoding tests for indexed/non-indexed indirect draws and indirect dispatches
- [x] 4.4 Build affected Aurora libraries and tests; record unavailable platform checks explicitly
- [x] 4.5 Update verification evidence without commit or archive

## 5. Review corrections

- [x] 5.1 Use mutually exclusive compute commands and one pure indirect argument-layout resolver
- [x] 5.2 Enable and report Vulkan logical-device features; fix Metal indirect validation and reject misaligned/count-overflow inputs
- [x] 5.3 Bound DX12 signature caching with explicit graph/direct lifetime owners and remove redundant native state
- [x] 5.4 Build available backends and run negative/encoding regressions; record unavailable GPU execution
