---
name: skyengine-aurora-reference
description: Quick index of the Aurora RHI architecture — module layout, RDG pass model, resource tiers, ECS scene, slang shader backend, backend implementations, and per-module test targets. Use when answering aurora design/structure questions or locating a component.
compatibility: opencode
metadata:
  source: aurora implementation + openspec changes (2026/03–2026/09)
  audience: contributors
---

# Aurora 架构速查

Aurora 是 SkyEngine 重写中的新 RHI 层（分支 `dev_refactor_rhi`，取代旧 `engine/rhi` + 旧 `engine/render`）。  
命名空间一律 **`sky::aurora`**（不新增 `aurora::rhi` 等别名——review 确认过的规则）。  
后端以动态库编译，经 `Instance::Init({api})` 运行时 dlopen 选择。

## 模块布局（engine/aurora/）

```
engine/aurora/
├── rhi/                    # RHI 接口 + 后端
│   ├── interface/          # 接口层 target: Aurora.RHI
│   │   └── include/aurora/
│   │       ├── rhi/        # Device/Image/Buffer/Shader/Pipeline*/ResourceGroup/Encoder/Queue/SwapChain/Barrier
│   │       └── rdg/        # RenderGraph/RenderGraphBuilder/CompiledGraph/RDG*/BatchAllocator/DeviceFrameDispatcher/RenderDeviceExclusive
│   ├── vulkan/             # Vulkan 后端 (dlopen: AuroraVulkan)
│   ├── dx12/               # DX12 后端 (AuroraDX12) —— PipelineLayout/ResourceGroup 仍是 stub（见 aurora-resource-group change）
│   ├── metal/              # Metal 后端 (AuroraMetal, macOS only)
│   └── test/               # target: AuroraRHITest
├── shader/                 # target: Aurora.Shader（slang 编译 + 布局单一事实源）
│   └── include/aurora/shader/
│       ├── RgBlockDesc.h          # RgBlockDesc：set/binding/fields 单一事实源
│       ├── gen/ShaderBlockGen.h   # → HLSL 文本 + ContentHash（离线 cache key）
│       ├── ShaderCompilerSlang.h  # Slang 编译器（SPIRV/DXIL/MSL 直出）
│       ├── ShaderReflection.h     # ShaderResource/ShaderBlockLayout（含 UBO 成员）
│       └── test/                  # target: AuroraShaderTest
├── core/                   # target: Aurora（scene ECS）
│   └── include/aurora/scene/
│       ├── SceneView.h     # frustum + view/viewProj 矩阵 + FrustumCulling + ViewSpaceDepth
│       ├── RenderScene.h   # EntityRegistry 内嵌；views 独立 registry
│       ├── SceneTypes.h    # Bounds/WorldInfo/Light/Skin（SKY_TYPE_TAG 注册）
│       └── test/           # target: AuroraCoreTest
└── pipeline/               # target: Aurora.Pipeline（pass template 层）
    └── include/aurora/pipeline/
        ├── PipelinePass.h             # OnSetup/BuildRDG/OnSceneChanged 三段式
        ├── SceneRasterPassTemplate.h  # queue 声明 + Collect（cull→tag→sort）
        ├── OpaquePass.h               # 示范 pass（color+depth + opaque queue）
        ├── GlobalRenderResources.h    # Global tier（set 0）UBO/RG + UpdateView
        ├── ReflectionValidation.h     # RgBlockDesc vs ShaderReflection 校验
        └── test/                      # target: AuroraPipelineTest
```

依赖方向：`aurora/shader → aurora/rhi`（**单向**，rhi 不可调 shader）；`core/pipeline → rhi + shader`。

---

## RDG（render graph，rhi/interface/rdg/）

三段式：**Setup（builder 声明）→ Compile（产出 CompiledGraph）→ Execute（只读 CompiledGraph）**。

- `RenderGraph::Build(device, FrameAllocator&)`：所有 setup 容器走 `TransientVector`（`FrameAllocator::Arena()`）。
- `RenderGraph::SetGlobalResourceGroup(ResourceGroup*)` → `CompiledGraph::globalResourceGroup`。
- 6 种 pass：`AddSceneRasterPass / AddFullScreenPass / AddComputePass / AddCopyBlitPass / AddPresentPass / AddCustomPass`（Custom 是唯一保留 `std::function` 的逃生门，用于 MetalFX/DLSS 类扩展）。
- `CompiledPass::payload` 是 `std::variant<SceneRasterPayload, FullScreenPayload, ComputePayload, CopyBlitPayload, PresentPayload, CustomPayload>`——无 std::function 热路径。
- **SceneRaster queue**：pass 内多 queue，每 queue 有 `name / techniqueTag / items / queueResourceGroup / sortPolicy`（NONE/FRONT_TO_BACK/BACK_TO_FRONT，纯数据标记，RDG 不排序）；`AddDrawItem(item)` 无 queue 参数 → 默认 queue 0（懒创建 "default"）。
- `DrawItem`：`pso / batchResourceGroup / batchDynamicOffset / vb/ib/offsets / CmdDrawIndexed`。
- **culling 种子**：import 资源 + `MarkOfInterest` 资源的 lastWriter + **PRESENT access 的 pass**（否则 present 被误杀）。
- barrier 扁平数组：每 pass 连续段 + `finalBarrierOffset` 帧末段。
- executor（`Execute.cpp`）按 pass 类型 dispatch；`PRESENT` 无额外 encoder 操作（barrier 已在 per-pass 段）。

---

## 三层 ResourceGroup（aurora-resource-tiers 已落地）

| tier | set | 持有方 | 更新 |
|---|---|---|---|
| Global | 0 | `GlobalRenderResources`（pipeline） | 每帧 `UpdateView(SceneView, time)` 写 UBO + RG.Update；executor 每 pass 绑 set 0 |
| Pass | 1 | `PipelinePass`（`GetPassBlocks()` → OnSetup 建 / OnSceneChanged 重建） | queue RG 优先于 pass RG |
| Batch | 2 | `BatchAllocator`（rdg/，per-frame dynamic UBO，256 对齐） | `DrawItem::batchDynamicOffset`；executor `BindResourceGroup(2, rg, 1, &offset)` |

**RgBlockDesc 单一事实源**：同一份 `{set, binding, blockName, fields[]}` 产出 RHI `ResourceGroupLayout` 与 HLSL header（std140 offset 表内置）；`ContentHash` 供离线 shader cache key（header 变更必须使 cache 失效）。

**三平台映射约定**：VK 直接 set/binding；DX12 set→root parameter（每 set 一个 descriptor table，小 cbuffer 可 root CBV）；Metal set→argument buffer index（Metal 3+）。

---

## Slang 编译通道（aurora/shader）

- `ShaderCompilerSlang::Compile(ShaderCompileDesc) -> ShaderCompileResult`：SPIRV / DXIL / MSL 直出（**Metal 不经 SPIRV-Cross**）。
- **ParameterBlock 布局**：普通资源（texture/sampler）→ set 0；每个 ParameterBlock 按声明序独占一个 set（gGlobal=1, gPass=2...）。MSL 侧 → `constant* T [[buffer(N)]]`。
- **反射**：只用 slang 自己的 program layout（slang SPIRV 喂 SPIRV-Cross 会崩——按「各平台反射自己产物」原则）。收敛到 `ShaderReflection{resources, blocks}`；blocks 含 UBO 成员 name/offset/size。
- `#line` 默认关闭（`TargetDesc.compilerOptionEntries` 设 `SLANG_LINE_DIRECTIVE_MODE_NONE`）。
- ⚠️ slang COM teardown 顺序脆弱 → spike 阶段主动泄漏（代码注释标注，转正时处理）。
- 3rd：slang v2026.9.2，选择性子模块 9 个，DXIL 走仓库自带 DXC 包供源（避网络下载）。

---

## ECS（core/ecs + aurora/core/scene）

- `EntityId`：24bit index + 8bit generation（防悬垂）。
- `SparseSet<T>`：sparse 页表 + dense 双数组（swap-remove 保连续）。
- `EntityRegistry`：free list + generation；type-erased `PoolHolder<T>` 懒建池。
- **`TypeId<T>()` 显式 tag 制**：`SKY_TYPE_TAG(T, "sky.<module>.<TypeName>")`；未注册类型 `static_assert` 编译失败（无 fallback，防跨模块静态分裂）。
- `EntityRegistry::View<Ts...>()`：多池交集迭代（运行时选最小池为主驱动，dense 扫 + 其余池 Contains）。
- scene 组件（`SceneTypes.h`）：`Bounds` / `WorldInfo`（纯 world 矩阵）/ `Light`（含 point/spot 参数）/ `Skin`（占位）。
- `RenderItem`（techniqueTag + DrawItem）**已按用户决定移出组件**；Collect 暂 cull-only（等 technique 分桶设计）。

---

## 测试 target（总入口 ctest）

| target | 模块 | 内容 |
|---|---|---|
| `AuroraRHITest` | rhi/test | RDG/barrier/encoder/device/sync/submit/shader 对象 |
| `AuroraShaderTest` | shader/test | SlangSpike（编译+反射）/ SlangVulkan/D3D12/Metal（共享 `SlangBackendTestCommon.h` 流程体）/ RgBlockDesc |
| `AuroraCoreTest` | core/test | scene ECS / SceneView culling / entity 生命周期 |
| `AuroraPipelineTest` | pipeline/test | OpaquePass / Global tier / BatchAllocator / ReflectionValidation |

后端 dll 以 `add_dependencies` 挂到测试 target（只保证构建，运行时 dlopen），**不直接链接**。

---

## 关键改动索引（openspec/changes/archive/）

| 主题 | change | 要点 |
|---|---|---|
| 帧 allocator | `core-frame-allocator` | bump + Mark/Rewind + 统计；RDG 全容器 TransientVector |
| RDG variant | `aurora-rdg-compiledgraph-variant` | CompiledPass variant payload 替代 std::function |
| pass builder | `aurora-rdg-pass-builder` | 6 pass 类型 + PRESENT culling 种子修正 |
| executor | `aurora-rdg-compiled-executor` | executor 只读 CompiledGraph（finalBarrierOffset） |
| pass template | `pipeline-pass-template` | PipelinePass 基类 + SceneRasterPassTemplate + OpaquePass + queue |
| scene collect | `aurora-scene-collect` | SceneView/RenderScene + queue techniqueTag |
| scene ECS | `aurora-scene-ecs` | scene 迁 aurora/core + ECS 化 |
| TypeId | `core-ecs-type-id-hash` | SKY_TYPE_TAG 显式 tag，无 fallback |
| View | `ecs-view-scene-components` | EntityRegistry::View + Light/WorldInfo |
| 三层 RG | `aurora-resource-tiers` | RgBlockDesc 单一事实源 + Global/Pass/Batch + 反射校验 + 三平台映射 |
| slang | `aurora-slang-spike` | Slang 三后端直出 + ParameterBlock/UBO 反射 + 模块测试拆分 |

## 已知未完成（跟进项）

- `aurora-resource-group`：DX12/Metal/GLES 的 ResourceGroup/PipelineLayout 实质实现（DX12 现为 stub，pipeline 对象创建验证因此暂缓）
- `aurora-queue-submit-present`：D3D12SwapChain、SwapChainTest（需 SDL 窗口）、跨平台验证
- technique/材质系统：决定 RenderItem 的最终形态与 scene 分桶
- slang COM teardown 稳定性（spike 期泄漏）
