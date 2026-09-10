## Why

三层 ResourceGroup（Global set 0 / Pass set 1 / Batch set 2）的绑定语义已在 RDG/executor 里定义，但**数据流没组织**：

- `CompiledGraph::globalResourceGroup` 字段存在但从无人赋值，executor 也不绑 set 0——Global 层是断的。
- Pass RG 靠上层手动传 `ResourceGroup*`，谁创建/何时更新没有约定。
- Batch RG 每 DrawItem 一个持久 RG 不现实（每帧 instance 数据在变），需要 dynamic UBO。
- 现有 shader 布局头（`assets/shaders/layout/*.hlslh`）是**手写**的，与 C++ 侧 ResourceGroupLayout 定义脱节——layout 改了 shader 不会跟着变。

## What Changes

### Part 1 — 单一事实源 `RgBlockDesc`（`aurora/pipeline/rg/`）

- **新增** `RgBlockDesc`：C++ 描述 `{ set, binding, blockName, fields[] }`（字段类型限于 scalar/vec2/vec3/vec4/mat4，对齐规则内置）。
- **新增** `RgBlockDesc -> ResourceGroupLayout`（驱动 RHI 创建 layout）与 `RgBlockDesc -> HLSL header 文本`（生成 `[[vk::binding(b, s)]] cbuffer/Texture2D/...` 声明）。同一份描述同时喂 RHI 与 shader，消除手写脱节。

### Part 2 — Global tier（set 0）

- **新增** `GlobalRenderResources`（aurora/pipeline 持有）：global UBO（view/proj/viewProj/cameraPos/time）+ global RG；每帧 `UpdateView(const SceneView&, time)` 写 UBO + `RG.Update`。
- **新增** `RenderGraph::SetGlobalResourceGroup(ResourceGroup*)` → `CompiledGraph::globalResourceGroup` 赋值。
- **修改** executor：每个 raster/compute pass 开始前绑 set 0（`BindResourceGroup(0, global)`）。

### Part 3 — Pass tier（set 1）

- `PipelinePass` 新增 pass RG 约定：`OnSetup` 由 pass 的 `RgBlockDesc` 创建 layout + RG（持久），`OnSceneChanged` 重建；`BuildRDG` 经 builder 传入（现有 `SetPassResourceGroup`/`SetQueueResourceGroup` wiring 不变）。

### Part 4 — Batch tier（set 2，dynamic UBO）

- **新增** per-frame dynamic UBO：`BatchAllocator`（单一大 buffer，offset 分配器，帧末 reset；`UNIFORM_BUFFER_DYNAMIC`）。
- **修改** `DrawItem` 新增 `batchDynamicOffset`；executor 绑 set 2 时传 dynamicOffsets。
- 收集方（Collector/未来 technique）写 batch 数据到 dynamic UBO 并记录 offset。

### Part 5 — 三平台 layout / 反射映射约定

- **约定**同一 `RgBlockDesc` 在三平台的映射：Vulkan 直接 set/binding；DX12 set → root parameter（每 set 一个 descriptor table，小 cbuffer 可 root CBV）；Metal set → argument buffer index（Metal 3+，SPIRV-Cross MSL 重映射）。
- **反射不统一**：每平台反射自己的编译产物——Vulkan 走 SPIRV-Cross 读 SPIR-V；DX12 走 DXC container reflection 读 DXIL；Metal 走 SPIRV-Cross MSL 通道的 `get_msl_resource_bindings()` 重映射表（评估结论：Metal 是唯一存在 (set,binding)→MSL index 重映射的平台，映射权威来源是 SPIRV-Cross 本身；`MTLRenderPipelineReflection` 需先建 PSO、依赖运行中设备、无 set/binding 概念、受 dead-strip 干扰——降级为 Metal-only debug 校验）。各通道产物统一收敛到 `engine/shader` 的 `ShaderReflection` 结构（通道不同、产物同构）；校验在每平台各自进行。
- **正向校验**：RgBlockDesc → layout → 本平台反射 → 比对 set/binding/类型一致性；反向推导（反射 → RgBlockDesc）留后续。
- **边界**：DX12/Metal 后端 ResourceGroup 实现属 `aurora-resource-group` change；本 change 只定约定 + Vulkan 落地 + Vulkan 反射校验。

## Capabilities

### New Capabilities

- `aurora-resource-tiers`: 三层 ResourceGroup 的数据流组织契约（RgBlockDesc 单一事实源、Global 更新路径、Pass 生命周期、Batch dynamic UBO）。

### Modified Capabilities

- `aurora-rdg`: executor 帧首/每 pass 绑 set 0；`DrawItem` 新增 `batchDynamicOffset`；`CompiledGraph::globalResourceGroup` 正式生效。

## Impact

- **aurora/pipeline**：新增 `rg/RgBlockDesc.h`、`rg/ShaderBlockGen.h/.cpp`、`rg/GlobalRenderResources.h/.cpp`、`rg/BatchAllocator.h/.cpp`；`PipelinePass` 加 pass RG 约定。
- **aurora RDG**：`CompiledGraph.h`（DrawItem + globalResourceGroup 生效）、`RenderGraph.h/.cpp`（SetGlobalResourceGroup）、`Execute.cpp`（set 0 绑定 + set 2 dynamic offset）。
- **测试**：RgBlockDesc→layout/header 生成断言；Global 更新 + RG Update（vulkan headless）；dynamic offset 透传到 payload；executor 绑定 smoke。
- **依赖**：`aurora-resource-group` 的 Vulkan 实现（已完成）；`engine/shader` 反射（SPIRV-Cross / DXC）。
- **v1 边界**：Batch dynamic UBO 单帧 buffer（无 in-flight 环形）；shader header 只出 HLSL；DX12/Metal 只定映射约定不写后端代码（后端实现属 `aurora-resource-group`）。
