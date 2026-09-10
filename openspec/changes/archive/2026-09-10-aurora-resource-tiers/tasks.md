## 1. RgBlockDesc 单一事实源（aurora/pipeline/rg/）

- [x] 1.1 `RgBlockDesc.h`：`RgFieldType`（FLOAT/FLOAT2/FLOAT3/FLOAT4/MAT4/TEXTURE2D/TEXTURE_CUBE/SAMPLER）+ `RgField` + `RgBlockDesc`
- [x] 1.2 `RgBlockDesc -> ResourceGroupLayout::Descriptor`（cbuffer→UNIFORM_BUFFER 或 DYNAMIC；texture/sampler→对应类型）；std140 对齐 offset 表内置
- [x] 1.3 `ShaderBlockGen.h/.cpp`：RgBlockDesc -> HLSL 文本（`[[vk::binding(b,s)]] cbuffer/Texture2D/SamplerState`）；纯库函数，运行时与离线 shader cache builder 共用；**提供 header 内容哈希（供 cache key 计入，header 变更必须使离线 cache 失效）**
- [x] 1.4 测试：layout 生成断言 + header 文本断言 + 两侧一致性 + header 内容哈希稳定性

## 2. Global tier

- [x] 2.1 `GlobalRenderResources.h/.cpp`：global UBO（view/proj/viewProj/cameraPos/time）+ global RG；`UpdateView(const SceneView&, float time)` 写 UBO + RG.Update
- [x] 2.2 `RenderGraph::SetGlobalResourceGroup(ResourceGroup*)`；`ProduceCompiledGraph` 写入 `CompiledGraph::globalResourceGroup`
- [x] 2.3 `Execute.cpp`：每 raster/fullscreen/compute pass 开始前 `BindResourceGroup(0, globalRG)`（非空时）

## 3. Pass tier

- [x] 3.1 `PipelinePass` 新增 `mPassBlockDesc` 约定 + `OnSetup` 创建 layout+RG（持久）+ `OnSceneChanged` 重建
- [x] 3.2 `OpaquePass` 示范接入（声明 pass block，BuildRDG 传 SetPassResourceGroup）

## 4. Batch tier（dynamic UBO）

- [x] 4.1 `BatchAllocator.h/.cpp`：per-frame dynamic UBO（host-visible，`UNIFORM_BUFFER_DYNAMIC`）；`Allocate(size)->offset`（256 对齐）/`Reset()` 帧末
- [x] 4.2 `DrawItem` 新增 `batchDynamicOffset`（默认 0）
- [x] 4.3 `Execute.cpp`：SCENE_RASTER 绑 set 2 时 `BindResourceGroup(2, batchRG, 1, &item.batchDynamicOffset)`
- [x] 4.4 `Compile.cpp`：`ProduceCompiledGraph` 透传 `batchDynamicOffset`

## 5. 三平台映射约定 + 反射校验

- [x] 5.1 `RgBlockDesc.h` 补三平台映射文档注释（VK 直接 set/binding；DX12 set→root parameter；Metal set→argument buffer index）
- [x] 5.2 反射校验：`BuildReflectionSPIRV` 反射结果（ShaderResource set/binding）与 RgBlockDesc 比对，不一致报错
- [x] 5.3 测试：手写 mini HLSL → 编译 SPIR-V → 反射 → 与 RgBlockDesc 一致

## 6. 测试

- [x] 6.1 `RgBlockDesc` 测试（layout + header + 一致性）
- [x] 6.2 Global 更新 + RG.Update（vulkan headless smoke）
- [x] 6.3 BatchAllocator 分配/对齐/reset 断言
- [x] 6.4 `batchDynamicOffset` 透传断言（setup → CompiledGraph）
- [x] 6.5 executor 绑定 smoke（不崩溃，global/batch RG 绑定路径走到）

## 7. 验证与收尾

- [x] 7.1 全量 `cmake --build` 通过
- [x] 7.2 `AuroraTest` + `CoreTest` 全绿
- [ ] 7.3 待用户确认后 `openspec archive aurora-resource-tiers` 归档本 change
