## Why

`MaterialTechnique` currently holds a full RHI `PipelineState`, which mixes state owned by three different things: the mesh owns `vertexBindings` / `vertexAttributes` (stream layout) and `inputAssembly` (topology); the draw pass owns `multiSample` (sample count); only depth/stencil, raster state, blending and alpha-to-coverage are intrinsic to the material. Letting the material define mesh/pass state couples it to one mesh layout and one pass.

## What Changes

- Add `MaterialRenderState` (material-intrinsic state only): `{ DepthStencil depthStencil; RasterState rasterState; std::vector<BlendState> blendStates; bool alphaToCoverage; }`.
- `MaterialTechnique` holds `MaterialRenderState renderState` instead of `PipelineState state`; remove mesh-owned (`vertexBindings`, `vertexAttributes`, `inputAssembly`) and pass-owned (`multiSample`) parts from what the material defines.
- `blendStates` is indexed by color target; a technique targets one pass, so a different pass / blend needs a separate technique.
- RHI `PipelineState` is unchanged; the renderer merges `MaterialRenderState` + mesh vertex input + pass attachment/sample state into it when creating the pipeline.
- Update `MaterialTest`.
- **BREAKING**: `MaterialTechnique` member changes from `PipelineState state` to `MaterialRenderState renderState`.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities
- `aurora-material`: `MaterialTechnique` defines only material-intrinsic render state (`MaterialRenderState`), not mesh- or pass-owned pipeline state.

## Impact

- Code: `engine/aurora/core/include/aurora/resource/Material.h`, `engine/aurora/core/test/MaterialTest.cpp`.
- No RHI/backend change (RHI `PipelineState` is untouched).
