## Context

GPU-driven features need two capabilities the current feature-facing graph API does not expose:

- Persistent, caller-owned GPU buffers that hold simulation state across frames.
- Draw/dispatch counts produced on the GPU through indirect arguments.

Internally `FrameGraphBuilder::Import` already accepts `BufferPtr` with initial/final state and initialization (`engine/aurora/core/src/renderer/graph/FrameGraphBuilder.h:21-23`), and the RHI already exposes `DrawIndirect`, `DrawIndexedIndirect` and `DispatchIndirect` (`engine/aurora/rhi/interface/include/aurora/rhi/Encoder.h:52-68`). The missing piece is the checked feature-facing seam and the declarative graph support.

## Goals / Non-Goals

**Goals:**

- Let a feature create persistent GPU buffers once and import them into each frame graph.
- Preserve caller ownership, reference counting and submission-lifetime retention.
- Add optional indirect raster draw and indirect compute dispatch declarations.
- Validate indirect arguments and gate on actual device features.
- Lower indirect declarations through Vulkan, DX12 and Metal compilers.

**Non-Goals:**

- No cross-frame caching of graph bindings or descriptor sets.
- No exposure of `Device` or queue submission to features.
- No mesh-shader or work-graph support.
- No change to the Custom pass escape hatch beyond what imported buffers enable.
- No particle/terrain/vegetation implementation in this change.

## Decisions

### Persistent buffer creation and import

Add to `FrameFeaturePrepareContext`:

```cpp
virtual std::variant<FrameError, BufferPtr>
CreatePersistentBuffer(const Buffer::Descriptor &) = 0;
```

Add to `FrameGraphDeclarations` (or `FrameGraphServices`):

```cpp
virtual std::variant<FrameError, GraphBufferHandle>
ImportBuffer(BufferPtr, CopyBufferState initial, CopyBufferState final, bool initialized) = 0;
```

Rationale:

- `Prepare` already runs on the coordinator with a real `Device` (`RendererRuntime.cpp:643-663`), so it is the natural place to create device resources.
- The feature owns the returned `BufferPtr` and calls `ImportBuffer` in `BuildView`/`BuildFrame`; the graph holds a reference until submission completes.
- `initial`/`final` and `initialized` reuse the existing `FrameGraphBuffer` contract, so analysis, lifetime and barriers stay in one place.
- The alternative — exposing `Device` to features — leaks backend ownership and lets features create resources outside graph accounting. Rejected.

Persistent buffers MUST be GPU-only or explicitly host-visible per `Buffer::Descriptor`; the import path does not add upload semantics. A feature that needs CPU-written data uses the existing `Upload`/staging path or maps its own host-visible buffer.

### Indirect raster draw

Add an optional field to `RasterDraw`:

```cpp
struct RasterIndirectDraw {
    GraphBufferHandle arguments;
    uint64_t          offset = 0;
    uint32_t          drawCount = 1;
    uint32_t          stride = 0;   // 0 means backend-tight packing
};

struct RasterDraw {
    // ...existing fields...
    std::optional<RasterIndirectDraw> indirect;
};
```

`indirect` and the existing `command`/`indexed` fields are mutually exclusive. The indirect argument buffer is always a read dependency and MUST be initialized by an earlier pass or import.

### Indirect compute dispatch

Add an optional field to `FrameComputePass`:

```cpp
struct ComputeIndirectDispatch {
    GraphBufferHandle arguments;
    uint64_t          offset = 0;
};

struct FrameComputePass {
    ShaderPtr shader;
    std::variant<CmdDispatch, ComputeIndirectDispatch> dispatch = CmdDispatch{};
    FrameBindingGroups groups;
    bool sideEffect = false;
};
```

Direct and indirect dispatch are distinct variant alternatives; an indirect caller does not also initialize unrelated explicit counts. A direct compute pass rejects zero dimensions.

### Validation

Compile rejects:

- Indirect arguments referencing an uninitialized or non-readable buffer.
- Out-of-range offset/stride/drawCount, misaligned strides and overflow. `ResolveIndirectArgumentLayout` provides the shared pure command-size/stride/span calculation; backends retain their own native synchronization and encoding.
- An indirect field combined with explicit draw/dispatch counts.
- Indirect use when the selected device lacks the required feature (`multiDrawIndirect`, `firstInstanceIndirect`, or backend-equivalent dispatch support).
- A raster indirect draw whose argument layout does not match indexed/non-indexed usage.

`drawCount` may be greater than one only when `multiDrawIndirect` is supported; otherwise it must be exactly one. Argument content itself is GPU-produced, so Compile validates layout and range, not the dynamic values.

### Native lowering

- Vulkan: `vkCmdDraw*Indirect` / `vkCmdDispatchIndirect`, using the graph barrier from the argument buffer write to the indirect-read stage. Report only features enabled on the logical device and enforce the physical draw-count limit.
- DX12: a bounded signature cache keyed by command kind and stride; graph plans and direct command buffers retain `ComPtr` owners through safe retirement/reset. `ExecuteIndirect` remains on the native direct list.
- Metal: `draw*Primitives:...indirectBuffer:` / `dispatchThreadgroupsWithIndirectBuffer:`; `stride == 0` maps to tightly packed arguments.
- Unsupported backend/feature combinations return `Unsupported`; no silent CPU fallback inside the graph.

### Lifetime and synchronization

- Imported buffers follow the existing `FrameGraphBuffer` rules: reference retention, content ranges, initial/final state and in-flight exclusion.
- The feature owns the `BufferPtr`; `OnRetired` releases it after all frames that imported it complete.
- A feature that writes persistent state every frame is responsible for per-in-flight-frame state rotation or for relying on the renderer's serialized submission. The graph does not invent cross-frame synchronization.

## Risks / Trade-offs

- **Indirect argument layout mismatches** -> central shape validation plus per-backend command-signature tests.
- **Device capability gaps on mobile/Metal** -> explicit `Unsupported`; features must provide a non-indirect fallback.
- **Persistent buffer lifetime bugs** -> graph retains imports until submission completion; tests drop caller references immediately after import.
- **In-flight state races** -> document per-frame rotation; test with multiple frames in flight.
- **Feature API growth** -> keep creation in `Prepare` and import in build; no general device access.

## Migration Plan

1. Add the API surface and additive graph fields.
2. Implement validation and analysis for imported buffers and indirect arguments.
3. Lower indirect in Vulkan, DX12 and Metal; add GPU readback and argument-shape tests.
4. Migrate no existing feature; current graph declarations remain valid.
5. Run affected Aurora libraries and tests on available backends; record unavailable platform checks explicitly.
