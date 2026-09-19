## Why

Metal 后端对比 Vulkan 后端存在一组阻塞级缺口，导致 macOS 上无法完成基础渲染：

1. `MetalDevice.mm` 调用不存在的 `MTLDevice.minConstantBufferAlignmentBytes`，直接编译失败。
2. slang MSL 输出保留原始 entry 名（`mainVS`/`mainFS`），`MetalShaderFunction` 却查找硬编码 `VSMain`/`FSMain`，slang→MSL→RHI 路径在 `newFunctionWithName` 处失败。
3. ResourceGroup 全栈缺失：`MetalResourceGroup` 类不存在、`CreateResourceGroup`/`CreateDescriptorBatch` 返回 nullptr、`MetalDescriptorEncoder`/`MetalDescriptorBatch` 是空壳、`BindResourceGroup` 是 TODO；pipeline 层（`GlobalRenderResources`、`PipelinePass`）与 RDG executor 均依赖它。
4. 深度/模板状态与光栅化状态（cull/frontFace/fill/depthBias）完全未接入，无深度测试。
5. compute `Dispatch`/`DispatchIndirect` 硬编码 threadgroup (1,1,1)，MSL 源码不携带 `numthreads`，任何非 (1,1,1) 的 kernel 结果错误。
6. `BeginRendering` 完全忽略 stencil attachment。
7. `PushConstants` 空壳（图形+计算）。

## What Changes

- **绑定模型（实测 slang v2026.9.2 MSL 输出定案）**：slang Metal target 把资源按类别扁平连续编号——buffer→`[[buffer(N)]]`、texture→`[[texture(N)]]`、sampler→`[[sampler(N)]]`，忽略 `[[vk::binding]]` 的 space；MSL 编译的反射 `binding` 即为该类别内索引。Metal ResourceGroup 因此不需要 argument buffer（tier-1 直接绑定）：记录 binding→资源 三张表，`BindResourceGroup` 时逐个 `setBuffer/setTexture/setSampler`。`RgBlockDesc.h` 中过时的「SPIRV-Cross remap」注释同步更新。
- **entry 名传递**：`ShaderFunction::Descriptor` 增加 `entry` 字段；Metal 用它查 `newFunctionWithName`，为空回退旧 `VSMain/FSMain/CSMain`（兼容手写 MSL 测试）；Vulkan 忽略（slang SPIRV 固定 `main`）。
- **threadgroup size**：`ShaderReflection` 增加 `threadGroupSize[3]`，slang 编译时从 entry point 反射 `getComputeThreadGroupSize` 填充；Metal compute pipeline 存取，`Dispatch`/`DispatchIndirect` 使用。
- **深度/模板 + 光栅化**：`MetalGraphicsPipeline` 创建 `MTLDepthStencilState` 并保存光栅化参数；`BindPipeline` 一并设置 `setDepthStencilState/setCullMode/setFrontFacingWinding/setTriangleFillMode/setDepthBias/setDepthClipMode/setStencil*ReferenceValue`。
- **stencil attachment**：`BeginRendering` 接 `stencilLoadOp/stencilStoreOp/clearStencil`，depth/stencil 按格式的 hasDepth/hasStencil 门控（对齐 Vulkan 行为）。
- **PushConstants**：slang MSL 把 `[[vk::push_constant]]` 发成普通 `constant T* [[buffer(N)]]`（实测）。约定 push constant 块在 shader 中最后声明 → 其槽位 = buffer 类资源数 - 1；`MetalShader` 从反射计算该槽位，encoder 用 `set*Bytes` 写入。同时 `ShaderCompilerSlang` 补记 `reflection.pushConstants`（PushConstantBuffer category，SPIRV/DXIL 受益）。
- **编译修复**：`minConstantBufferAlignmentBytes` 改为常量 256。
- **draw 拓扑**：`Draw*` 使用 pipeline 的 topology，不再硬编码 triangle。

## Capabilities

### New Capabilities

- `aurora-rhi-metal`: Metal 后端的 shader 加载（entry 名）、直接绑定 ResourceGroup、深度/模板/光栅化状态、compute dispatch、push constant 槽位约定。

### Modified Capabilities

（无 —— 接口字段为新增，不改变既有后端行为。）

## Impact

- **接口新增字段**（向后兼容）：`ShaderFunction::Descriptor.entry`、`ShaderReflection.threadGroupSize`。
- **修改文件**：`engine/aurora/rhi/interface`（Shader.h、ShaderReflection.h）、`engine/aurora/shader/ShaderCompilerSlang.cpp`、`engine/aurora/rhi/metal/*`（Shader/PipelineState/Encoder/Device/Utils + 新增 MetalResourceGroup，DescriptorEncoder/Batch 变真实实现）。
- **约定**：Metal 目标下 push constant 块必须最后声明；RgBlockDesc 的 binding 必须等于 slang MSL 类别内索引（声明顺序）。
- **验证**：`AuroraMetalTest`（SlangToRhiShaderObjects、SlangToRhiComputePipeline）在 macOS 通过；引擎编译通过。
