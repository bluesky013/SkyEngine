## 1. `MaterialRenderState`

- [x] 1.1 Add `MaterialRenderState { DepthStencil depthStencil; RasterState rasterState; std::vector<BlendState> blendStates; bool alphaToCoverage = false; }` to `aurora/resource/Material.h`
- [x] 1.2 Change `MaterialTechnique` from `PipelineState state` to `MaterialRenderState renderState`; update the file header comment
- [x] 1.3 Ensure the material no longer exposes `vertexBindings` / `vertexAttributes` / `inputAssembly` / `multiSample`

## 2. Tests and verification

- [x] 2.1 Update `MaterialTest.Technique` for the new technique shape (default `renderState`, `alphaToCoverage`)
- [x] 2.2 Build `Aurora.CoreTest`
- [x] 2.3 Run `MaterialTest.*`
