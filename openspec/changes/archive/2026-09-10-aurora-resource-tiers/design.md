## Context

三层 RG 绑定语义已定（set 0/1/2），Vulkan ResourceGroup 可用，dynamic UBO 支持已存在（`UNIFORM_BUFFER_DYNAMIC` + `BindResourceGroup(..., dynamicOffsets)`）。缺口：数据流组织 + layout/shader 单一事实源。

旧 shader 惯例参考：`assets/shaders/layout/*.hlslh`（手写，`[[vk::binding(b, s)]] cbuffer : register(b, spaceS)`）。

## Goals / Non-Goals

**Goals:**

- `RgBlockDesc` 单一事实源：同一份描述产出 RHI `ResourceGroupLayout` 与 HLSL header。
- Global tier：pipeline 层 `GlobalRenderResources` 持有，每帧从 SceneView 更新；executor 补 set 0 绑定。
- Pass tier：PipelinePass 持有 pass RG（OnSetup 创建 / OnSceneChanged 重建）。
- Batch tier：per-frame dynamic UBO + `DrawItem::batchDynamicOffset`，executor 绑 set 2 带 offset。

**Non-Goals:**

- 不做 PipelineLayout/PSO 组装（shader 管线接入后另起 change）。
- 不做 in-flight 环形 dynamic UBO（v1 单帧 buffer + reset）。
- 不做 GLSL 生成（只 HLSL；GLES 后续）。
- 不改 DX12/Metal/GLES 后端（v1 只 Vulkan）。
- 不做 material/technique 系统（batch 数据写入方后续）。

## Decisions

### 1. RgBlockDesc 单一事实源

```cpp
enum class RgFieldType : uint8_t { FLOAT, FLOAT2, FLOAT3, FLOAT4, MAT4, TEXTURE2D, SAMPLER, TEXTURE_CUBE };

struct RgField { RgFieldType type; Name name; };

struct RgBlockDesc {
    uint32_t set;
    uint32_t binding;
    Name     blockName;          // cbuffer 名 / 资源名
    std::vector<RgField> fields; // cbuffer 内字段（仅 scalar/vec/mat4）
};
```

- `RgBlockDesc -> ResourceGroupLayout::Descriptor`：cbuffer → UNIFORM_BUFFER（或 DYNAMIC）；texture/sampler → 对应 DescriptorType。
- `RgBlockDesc -> HLSL 文本`：`[[vk::binding(b, s)]] cbuffer Name : register(bB, spaceS) { ... }`。
- 同一份 desc 同时喂两边；测试断言文本与 layout 一致。

**header 生成时机（用户拍板：不落盘，虚拟 include）**：

- `ShaderBlockGen` 是纯库函数（`GenerateHlslHeader(const RgBlockDesc&) -> std::string`），运行时与离线 shader cache builder 共用同一份生成代码（cache builder 在同 repo，直接链接调用）。
- shader 编译经 `ShaderFileSystem` 虚拟 include 注入生成文本，不产出任何落盘文件。
- **cache key 必须计入 header 内容哈希**（或生成器版本号），否则 header 变更后离线 cache 不失效——这是该方案的硬约束。
- RgBlockDesc 注册表是静态的（引擎代码内写死的三层 block 定义），生成内容确定性，无运行时漂移。

### 2. Global tier 数据流

```
SceneView（view/proj 矩阵）
    │ 每帧
    ▼ GlobalRenderResources::UpdateView(view, time)
    │   写 global UBO（CPU map → memcpy）
    ▼ RG.Update({binding=0, buffer=ubo})
    │
    ▼ RenderGraph::SetGlobalResourceGroup(rg)   // BuildRDG 时
    ▼ CompiledGraph.globalResourceGroup
    ▼ executor: 每 pass 前 BindResourceGroup(0, rg)
```

归属 aurora/pipeline（用户拍板 pipeline 层）；`RenderDeviceExclusive` 或上层持有其指针传入 BuildRDG 流程。

### 3. Pass tier 生命周期

`PipelinePass` 新增成员：`mPassBlockDesc`（子类声明）→ `OnSetup` 建 layout+RG；`OnSceneChanged` 重建；`BuildRDG` 传 `SetPassResourceGroup(mPassRG)`。已有 wiring 不动。

### 4. Batch dynamic UBO

```cpp
class BatchAllocator {           // per-frame
    Buffer *ubo;                 // UNIFORM_BUFFER_DYNAMIC, host-visible
    uint32_t Allocate(uint32_t size);  // 返回 offset（256 对齐）
    void Reset();                // 帧末
};

DrawItem { ..., ResourceGroup *batchRG; uint32_t batchDynamicOffset; }
// executor: BindResourceGroup(2, batchRG, 1, &item.batchDynamicOffset)
```

收集方写 `ubo + offset` 并记录 offset 进 DrawItem。v1 单帧 buffer（EndFrame 重置 offset），in-flight 环形后续。

### 5. executor set 0 绑定时机

每 pass 开始前绑（非帧首一次）：pass 间 PSO 切换不改变 set 0 绑定，但保守起见每 pass 绑一次开销可忽略，且 CustomPass 可能改绑。

## Risks / Trade-offs

- **[RgBlockDesc 表达力]** v1 只支持 cbuffer 字段 + texture/sampler；数组/结构化 buffer 后续。→ 引擎类型先覆盖现有手写 header 的常用形态。
- **[std140 对齐]** 字段布局按 std140（vec3 占 16B 对齐）。生成器内置对齐计算；C++ 侧 UBO 写入用同一生成器的 offset 表，避免两套布局。
- **[BatchAllocator 与 FrameAllocator 重复]** 一个是 CPU arena 一个是 GPU buffer，语义不同不合并。

### 6. 三平台 layout / root signature / 反射映射

同一 `RgBlockDesc`（set/binding）在三平台的落地：

| 平台 | set/binding 映射 | PipelineLayout 组装 | 反射通道（各平台自己的） |
|---|---|---|---|
| Vulkan | set → `VkDescriptorSetLayout`，binding 直接对应 | `PipelineLayout = 有序 [set0, set1, set2] layout` | SPIRV-Cross 反射 SPIR-V（engine/shader 已有） |
| DX12 | set → root parameter index（每 set 一个 descriptor table）；`register(bN, spaceS)` 按 space 归入对应表；小 cbuffer（global/view）可用 root CBV 省 descriptor heap | `RootSignature = [table(set0), table(set1), table(set2)]`（或混合 root CBV） | DXC `IDxcContainerReflection` 反射 DXIL（engine/shader 已有） |
| Metal | 无 set 概念；set → argument buffer index（`[[buffer(N)]]`，Metal 3+）；`[[vk::binding]]` 经 SPIRV-Cross MSL 输出时重映射 | `[[buffer(0/1/2)]]` 三个 argument buffer | SPIRV-Cross MSL 通道：`CompilerMSL::get_msl_resource_bindings()` 在生成 MSL 的同一次编译中产出的 (set,binding)→MSL index 重映射表（离线可用、无设备依赖、映射与产物同源） |

**反射不统一（用户拍板）**：每个平台反射自己的编译产物——

- **Vulkan**：SPIRV-Cross 读 SPIR-V。
- **DX12**：DXC container reflection 读 DXIL（不经 SPIR-V 中转）。
- **Metal**：SPIRV-Cross MSL 通道的重映射表（评估结论：三平台中只有 Metal 存在 (set,binding)→MSL index 的重映射，该映射由 SPIRV-Cross 生成，权威来源必须是 SPIRV-Cross 本身；`MTLRenderPipelineReflection` 需先建完整 PSO、依赖运行中 MTLDevice、无 set/binding 概念且会被 dead-strip 干扰——降级为 Metal-only debug 校验 dead-strip 的可选手段，非正式通道）。

各通道反射结果统一收敛到 `engine/shader` 的 `ShaderReflection`（`ShaderResource{set,binding,type,...}`）结构——**通道不同、产物同构**。校验在每平台各自进行（reflect 本平台产物 → 与 RgBlockDesc 比对），不存在跨平台共享同一份反射结果的假设。

**与既有基建的关系：** `engine/shader` 的 `ShaderReflection` / `ShaderResource{set, binding, ...}` 作为各通道的统一产物结构直接复用；反射通道各自实现（SPIRV-Cross / DXC / Metal API）。`sl::ResourceGroupDecl` + `HLSLResourceDeclGenerator`（node 版）与 `RgBlockDesc` 的关系：v1 两套并存（RgBlockDesc 面向运行时 RDG 三层，sl 面向 shader 编辑器 node），后续收敛。

**后端依赖边界：** DX12/Metal 的 ResourceGroup 后端实现属 `aurora-resource-group` change（3.x/4.x，未实现）；本 change 只定义映射约定 + Vulkan 落地 + 反射校验，DX12/Metal 侧代码不动。

## Migration Plan

1. `aurora/pipeline/rg/`：RgBlockDesc + ShaderBlockGen（HLSL，三平台 binding 惯例统一走 `[[vk::binding(b,s)]]`/`register(bN, spaceS)`）+ 测试。
2. GlobalRenderResources + `RenderGraph::SetGlobalResourceGroup` + executor set 0。
3. PipelinePass pass RG 约定（OpaquePass 示范）。
4. BatchAllocator + DrawItem offset + executor set 2 dynamicOffsets。
5. 反射校验路径：Vulkan 走 SPIRV-Cross（本 change 落地）；DX12 走 DXC container reflection、Metal 走 SPIRV-Cross MSL 重映射表（通道约定写明，后端实现属 `aurora-resource-group`）；`MTLRenderPipelineReflection` 仅作 Metal-only debug 校验 dead-strip 的可选手段。
6. 测试全绿；archive。
