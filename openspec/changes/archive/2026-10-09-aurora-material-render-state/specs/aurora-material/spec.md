## MODIFIED Requirements

### Requirement: Material 内部按 technique 维护

`Material`（`RefObject`）SHALL 内部持有 `std::vector<MaterialTechnique>`，每个 `MaterialTechnique { Name techniqueTag; CounterPtr<Shader> shader; MaterialRenderState renderState; }`。SHALL 提供 `AddTechnique` / `GetTechniques` / `GetTechnique(const Name &tag)`（按 tag 查，未找到返回 `nullptr`）。

`MaterialRenderState` SHALL 只包含材质自有的固定功能状态：`{ DepthStencil depthStencil; RasterState rasterState; std::vector<BlendState> blendStates; bool alphaToCoverage; }`；SHALL NOT 包含 mesh 自有的顶点输入 / 拓扑（`vertexBindings` / `vertexAttributes` / `inputAssembly`）或 pass 自有的 `multiSample`（采样数）。

`blendStates` SHALL 按 color target 索引；一个 technique 对应一个 pass，需要不同 pass / blend SHALL 另起 technique。

`Material` SHALL NOT 引入顶层 `Technique` 资源类，SHALL NOT 依赖 `RenderTechniqueLibrary` 之类查表库。

#### Scenario: 添加并按 tag 查 technique

- **WHEN** `material.AddTechnique({Name("opaque"), shader, renderState})`
- **THEN** `GetTechniques().size() == 1`；`GetTechnique(Name("opaque"))->shader == shader`；`GetTechnique(Name("missing")) == nullptr`

#### Scenario: technique 只含材质自有渲染状态

- **WHEN** 阅读 `MaterialTechnique` / `MaterialRenderState`
- **THEN** `MaterialTechnique` 持 `MaterialRenderState`（`depthStencil` / `rasterState` / `blendStates` / `alphaToCoverage`）；不含 `vertexBindings` / `vertexAttributes` / `inputAssembly` / `multiSample`

#### Scenario: 无顶层 Technique 资源

- **WHEN** 阅读 `aurora/resource/Material.h`
- **THEN** 无顶层 `Technique` 类 / `RenderTechniqueLibrary`；technique 为 material 内部结构
