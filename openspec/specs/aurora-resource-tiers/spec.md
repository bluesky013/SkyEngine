# aurora-resource-tiers Specification

## Purpose
TBD - created by archiving change aurora-resource-tiers. Update Purpose after archive.
## Requirements
### Requirement: RgBlockDesc 单一事实源

`RgBlockDesc` SHALL 描述一个 resource block：`{ set, binding, blockName, fields[] }`。其 `set/binding/kind/fields` SHALL 由 `.slang` shader 反射生成（而非手写），同一份 desc SHALL 可产出：

- RHI `ResourceGroupLayout::Descriptor`（cbuffer → `UNIFORM_BUFFER`/`UNIFORM_BUFFER_DYNAMIC`；texture/sampler → 对应 `DescriptorType`）
- 供 shader `#include` 的 Slang 共享头（struct + `ParameterBlock` + `[[vk::binding]]`）
- C++ 镜像 struct（与 shader 布局一致，带 `static_assert` 校验）

C++ 侧 UBO 写入与 shader 声明 SHALL 使用同一 offset 表（offset 来自 slang 反射）。

#### Scenario: 反射派生 desc

- **WHEN** 对含 `[[vk::binding(0, 0)]] ParameterBlock<GlobalParams> gGlobal` 的 `.slang` 做 codegen
- **THEN** 生成 `RgBlockDesc{set=0, binding=0, blockName="Global", kind=CBUFFER}`，字段与 `GlobalParams` 成员一致

#### Scenario: 一致性

- **WHEN** 同一 `.slang` 反射分别产出 layout、shader 头、C++ struct
- **THEN** binding 编号、字段顺序、类型、offset 在两侧一致

### Requirement: Global tier 数据流

`GlobalRenderResources`（aurora/pipeline）SHALL 持有 global UBO 与 global RG，提供 `UpdateView(const SceneView&, float time)`（每帧写 UBO + RG.Update）。

`RenderGraph::SetGlobalResourceGroup(ResourceGroup*)` SHALL 把 global RG 写入 `CompiledGraph::globalResourceGroup`；executor SHALL 在每个 raster/compute pass 开始前 `BindResourceGroup(0, globalRG)`。

#### Scenario: 每帧更新
- **WHEN** `UpdateView(view, time)` 后 `Compile` + `Execute`
- **THEN** executor 在每 pass 前绑定 global RG 到 set 0

### Requirement: Pass tier 生命周期

`PipelinePass` SHALL 支持声明 pass 级 `RgBlockDesc`；`OnSetup` 由其创建 layout + RG（持久），`OnSceneChanged` 重建；`BuildRDG` 经 builder 传入（现有 SetPassResourceGroup/SetQueueResourceGroup wiring 不变）。

#### Scenario: pass RG 持久复用
- **WHEN** pass 连续多帧 BuildRDG 且未 OnSceneChanged
- **THEN** pass RG 复用同一实例

### Requirement: Batch tier dynamic UBO

`BatchAllocator` SHALL 管理 per-frame dynamic UBO：`Allocate(size) -> offset`（256 对齐）、`Reset()`（帧末）。`DrawItem` SHALL 含 `batchDynamicOffset`；executor 绑 set 2 SHALL 传 dynamicOffsets。

#### Scenario: 两个 draw 共用 RG 不同 offset
- **WHEN** 两个 DrawItem 同 batchRG、不同 `batchDynamicOffset`
- **THEN** executor 两次 `BindResourceGroup(2, rg, 1, &offset)` 分别使用各自 offset

