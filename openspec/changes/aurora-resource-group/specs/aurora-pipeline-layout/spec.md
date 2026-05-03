## ADDED Requirements

### Requirement: PipelineLayout 是 Pipeline 创建的必填依赖

`PipelineLayout` SHALL 是接口层一等公民，承担：
- 描述 pipeline 在 shader 中能引用的全部 ResourceGroup（按 set index）
- 描述 push constants 范围与可见 stage
- 在 4 个后端中分别映射为该后端的 binding 签名（`VkPipelineLayout` / `ID3D12RootSignature` / Metal argument-buffer slot map / GLES uniform-binding 表）

PipelineLayout 一旦创建即不可变；可被多个 GraphicsPipeline / ComputePipeline 共享。生命周期由 RefObject 管理；只要任何 Pipeline 还引用它，它就不会被销毁。

#### Scenario: 共享同一 layout 创建多个 pipeline
- **WHEN** 用同一 `PipelineLayout *layout` 创建 2 个 GraphicsPipeline 与 1 个 ComputePipeline
- **THEN** 三个 pipeline 创建均成功；销毁两个 pipeline 后 layout 仍存活（因第三个仍引用）

### Requirement: layout-pipeline 兼容性约束

GraphicsPipeline / ComputePipeline 创建时，后端 SHOULD 校验 shader reflection（如可用）与 PipelineLayout 一致：
- shader 引用的 set / binding 必须存在于 layout 中
- 类型必须匹配（shader uniform → layout UNIFORM_BUFFER 等）
- 类型不匹配在 debug build assert 或 logger 报警；release build 仍创建（VK / DX12 driver 自己会报）

本 change 内 reflection 是 best-effort；如后端没有 reflection 信息，校验可跳过。

#### Scenario: shader 引用未声明的 binding
- **WHEN** shader 使用 set=0 binding=5，但 layout 中 set 0 仅含 binding 0/1
- **THEN** Debug build 下 logger 报警（"shader uses undeclared binding"）；Release build 行为由 driver 决定

### Requirement: Encoder::BindPipeline 后续 BindResourceGroup 的 set 索引上限由 layout 决定

`BindResourceGroup(set, group, ...)` 的 `set` MUST < 当前已 BindPipeline 的 pipeline 对应 layout 的 `groups.size()`。

#### Scenario: set 索引超限
- **WHEN** layout 仅 1 个 group（set=0），调用 `BindResourceGroup(1, group, ...)`
- **THEN** Debug build assert；Release build 行为未定义但 MUST 不静默成功

### Requirement: 跨后端 binding 索引一致性约定

Aurora SHALL 保证：在 layout 中声明的 (set, binding) 索引在所有后端表现一致：

- Vulkan：直接 `layout(set=N, binding=M)`
- DX12：通过 root signature 的 register space + register 映射（space=N, register=M），调用方用 DXC `-fvk-` 或 root sig 模式编译
- Metal：argument buffer slot=N，buffer/texture index 由 layout 表内部分配
- GLES：拍平后由 binding 索引唯一确定 uniform location / texture unit

调用方写一份 layout，4 后端 binding 行为一致；shader 编译产物可不同（每后端单独编译），但 layout 描述统一。

#### Scenario: 同一 layout 跨后端
- **WHEN** 用同一 layout（set 0：binding 0=UB, binding 1=COMBINED）+ 各自后端编译的 shader 创建 pipeline
- **THEN** 4 个后端中 BindResourceGroup + Draw 行为一致（采到同一纹理、读到同一 UB 数据）
