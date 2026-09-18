## ADDED Requirements

### Requirement: PipelineState 携带顶点输入布局

`PipelineState` SHALL 携带 `std::vector<VertexBindingDesc> vertexBindings` 与 `std::vector<VertexAttributeDesc> vertexAttributes`。`VertexAttributeDesc` SHALL 含 `semantic`（名称）与 `semanticIndex`（序号，供 DX12 `SemanticName` + `SemanticIndex` 使用）。后端 PSO 创建 SHALL 能从该布局派生原生输入布局（输入装配路径）。

#### Scenario: 布局随 pipeline state 传递

- **WHEN** 调用方在 `PipelineState` 中声明 binding（stride/inputRate）与 attribute（location/binding/offset/format/semantic/semanticIndex）
- **THEN** `GraphicsPipeline::Descriptor::state` 中可读到相同布局

#### Scenario: 未声明布局时为空

- **WHEN** 不设置 `vertexBindings` / `vertexAttributes`
- **THEN** 两容器为空，后端按空输入布局创建（兼容 fullscreen / 顶点拉取路径）

### Requirement: Buffer 暴露分配大小

`Buffer` SHALL 提供 `GetSize()`（默认返回 0，后端可覆写）。需要按 buffer 推导视图大小的后端（如 DX12 index buffer view）SHALL 使用该访问器，SHALL NOT 依赖外部传入的 `range` 或硬编码 0。

#### Scenario: 后端返回真实大小

- **WHEN** 查询一个已创建 buffer 的 `GetSize()`
- **THEN** 返回其分配的字节数

#### Scenario: 默认实现

- **WHEN** 某后端未覆写 `GetSize()`
- **THEN** 返回 0，调用方据此跳过大小推导
