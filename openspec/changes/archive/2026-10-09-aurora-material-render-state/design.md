## Context

`Material` (`engine/aurora/core/include/aurora/resource/Material.h`) holds `std::vector<MaterialTechnique>`; currently `MaterialTechnique { Name techniqueTag; CounterPtr<Shader> shader; PipelineState state; }`. RHI `PipelineState` (`engine/aurora/rhi/interface/include/aurora/rhi/Core.h`) is the full graphics-PSO create-info: `depthStencil`, `multiSample`, `inputAssembly`, `rasterState`, `blendStates`, `vertexBindings`, `vertexAttributes`.

Those fields have three different owners:

| PSO field | Owner |
|---|---|
| shader stages | Material / Technique |
| `depthStencil`, `rasterState`, `blendStates`, alpha-to-coverage | Material |
| `vertexBindings`, `vertexAttributes`, `inputAssembly.topology` | Mesh (stream layout / topology) |
| `multiSample.sampleCount`, attachment formats/count | Draw pass |

`aurora-renderer` (the pipeline/主循环) is not implemented yet, so `MaterialTechnique.state` has no consumers — this is the point to correct the boundary. Constraints: header-only, `namespace sky::aurora`, and the RHI `PipelineState` is to be left untouched.

## Goals / Non-Goals

**Goals:**

- `MaterialTechnique` defines only the render state intrinsic to the material.
- Keep the RHI `PipelineState` as the complete PSO create-info, assembled by the renderer.

**Non-Goals:**

- No split of the RHI `PipelineState` into explicit sub-states.
- No renderer/pipeline merge implementation (future work).
- No material asset serialization of the render state (separate change).

## Decisions

### D1: `MaterialRenderState` holds only material-intrinsic state

```cpp
struct MaterialRenderState {
    DepthStencil            depthStencil;
    RasterState             rasterState;   // no topology / vertex input
    std::vector<BlendState> blendStates;   // per color target
    bool                    alphaToCoverage = false;
};
struct MaterialTechnique {
    Name                techniqueTag;
    CounterPtr<Shader>  shader;
    MaterialRenderState renderState;
};
```

- **Rationale**: exactly the fixed-function state that belongs to the material; mesh- and pass-owned parts are removed.
- **Alternative**: keep `PipelineState` in the technique and document ownership — rejected, the material would still be able to define mesh/pass state.

### D2: `blendStates` indexed by color target; one technique per pass

`blendStates[i]` is the blend for color attachment `i`. A technique targets one pass; needing a different pass or blend means a separate technique.

- **Rationale**: matches how a material is authored per pass; keeps the material from encoding pass structure.

### D3: `alphaToCoverage` belongs to the material

- **Rationale**: it expresses "this material's fragment alpha modulates coverage" (foliage / alpha-test), a shading intent tied to the material output. Unity exposes it as a material (`AlphaToMask`) state. It is only effective under MSAA, but that is a runtime precondition, not ownership.
- **Consequence**: `sampleCount` stays pass-owned; the renderer emits A2C only when the pass is multisampled.
- **Alternative**: pass-owned — rejected; materials could not express foliage A2C intent.

### D4: topology / vertex input belong to the mesh

`inputAssembly`, `vertexBindings`, `vertexAttributes` are removed from the material definition: they depend on the mesh stream layout and definition; fullscreen passes supply them from the pass side.

- **Rationale**: a material shared across meshes with different vertex layouts must not pin one layout.

### D5: RHI `PipelineState` unchanged; the renderer merges

The renderer (future) builds the RHI `PipelineState` by merging the technique's `MaterialRenderState` + mesh `{vertexBindings, vertexAttributes, topology}` + pass `{sampleCount, attachment formats/count}` + the selected shader.

- **Rationale**: least coupling; the RHI keeps a single complete PSO descriptor, and the ownership split lives at the material/renderer boundary.

## Risks / Trade-offs

- **[No renderer yet]** The merge is not exercised. → Mitigation: the contract is documented; `MaterialTechnique` only carries material state.
- **[`blendStates` count vs pass targets]** The material must supply blend states matching the pass target count. → Mitigation: technique↔pass binding is explicit; mismatch is handled by the renderer.
- **[A2C under non-MSAA]** `alphaToCoverage` is meaningless when the pass is not multisampled. → Mitigation: the renderer only feeds it into the PSO when `sampleCount > 1`.

## Migration Plan

1. `Material.h`: add `MaterialRenderState`; change `MaterialTechnique` to hold `renderState`.
2. Update `MaterialTest`.
3. Build `Aurora.CoreTest`; run `MaterialTest.*`.
4. Rollback: revert the header + test (header-only, no ABI/migration).

## Open Questions

- Should the renderer introduce a `MeshRenderState` (vertex buffers/attributes/topology) type, or derive it directly from `Mesh`?
- Where does the pipeline merge live (pipeline tier vs RDG pass)?
