## Context

用构建好的 libslang（v2026.9.2）实测 MSL 输出（probe  shader 含 `[[vk::binding]]` ParameterBlock / Texture2D / Sampler / RWStructuredBuffer / push_constant / numthreads）：

1. **资源索引扁平化**：slang Metal target 忽略 `vk::binding` 的 space 与显式 index，按声明顺序为每个类别分配连续索引：buffer→`[[buffer(0..)]]`、texture→`[[texture(0..)]]`、sampler→`[[sampler(0..)]]`。反射 `var->getOffset(MetalBuffer/MetalTexture/SamplerState)` 与 `getBindingIndex()` 返回该类别内索引，`getBindingSpace()` 恒为 0。
2. **entry 名保留**：MSL 函数名 = slang entry 名（`mainVS`/`mainFS`/`mainCS`）。
3. **push constant 物化**：`[[vk::push_constant]]` 在 Metal 上就是普通 `constant T* [[buffer(N)]]`，反射 category 退化为 `ConstantBuffer`，与普通 cbuffer 不可区分。
4. **numthreads 丢失**：MSL 源码不含线程组尺寸；只能经 slang entry point 反射 `getComputeThreadGroupSize` 获取。

引擎约定（`RgBlockDesc.h`）：Vulkan set→descriptor set、binding 直用；DX12 set→root parameter。Metal 旧注释假设 argument buffer + SPIRV-Cross remap 表，已与 slang 直发 MSL 的现实不符。

## Goals / Non-Goals

**Goals:**

- Metal 后端能创建 slang 编译的 shader/pipeline，完成带深度测试的 draw 与正确 threadgroup 的 dispatch。
- ResourceGroup/DescriptorEncoder/DescriptorBatch/BindResourceGroup 全栈可用（tier-1 直接绑定）。
- 接口改动向后兼容（新增字段，不改签名）。

**Non-Goals:**

- Metal argument buffer tier-2 / bindless heap（`DescriptorHeap` 两端均未实现，维持 stub）。
- `BlitImage`/`ResolveImage`、像素格式覆盖扩展、specialization constants（MSL `MTLFunctionConstantValues`）、swapchain 多 drawable——特性级，后续 change。
- barrier 语义、CommandPool::Reset 等打磨项。

## Decisions

### D1: ResourceGroup = 三张稀疏表，直接绑定，不做 argument buffer

slang 已经把 (set,binding) 折叠成类别内连续索引，argument buffer 只是多一层间接。`MetalResourceGroup` 持有：

- `buffers: binding -> {MTLBuffer, offset}`
- `textures: binding -> MTLTexture`
- `samplers: binding -> MTLSamplerState`

`Write*`（经 MetalDescriptorEncoder 或 MetalDescriptorBatch）立即写入表内；`BindResourceGroup` 遍历三张表，对图形 encoder 同时 setVertex*/setFragment*，对计算 encoder set*。资源用 `CounterPtr` 持有，对齐 `IDelayReleaseResource` 语义。`set` 参数与 `ImageLayout` 在 Metal 上忽略；dynamicOffsets 暂不支持（Metal 反射无 dynamic buffer 概念，遇到记日志）。

代价：多次 Bind 不同 group 会重复设置相同槽位——Metal 侧开销极小，可接受；tier-2 优化留给后续。

### D2: push constant 槽位 = buffer 类资源数 - 1（约定最后声明）

slang 按声明顺序分配 buffer 索引，且 Metal 反射无法区分 push constant 与普通 cbuffer，因此采用约定：shader 中 push constant 块最后声明。`MetalShader::Init` 从反射统计 UNIFORM/STORAGE buffer 资源数 N，`pushConstantSlot = N - 1`；encoder `PushConstants` 用 `setVertexBytes/setFragmentBytes/setBytes`（按 stageFlags）写到该槽位（≤4KB，Metal setBytes 限制内）。无 push constant 的 shader 不会被调 PushConstants（上层按 RgBlockDesc 决定），不存在误写。

备选：固定槽位 30（原 TODO 注释）——需要 slang 支持指定索引，slang 不提供；SPIRV-Cross remap——已与直发 MSL 路线矛盾。均否决。

### D3: entry 名经 `ShaderFunction::Descriptor.entry` 传递

接口加字段而非改名约定：slang 保留原名，重命名需要 slang 侧干预且 SPIRV 固定 `main` 无需干预。Metal 查 `entry`，空串回退 `VSMain/FSMain/CSMain`（兼容 `rhi/test/ShaderTest.cpp` 手写 MSL）。测试 helper `MakeShaderFuncDesc` 补 entry 参数。

### D4: threadGroupSize 进 ShaderReflection

`ShaderReflection` 加 `uint32_t threadGroupSize[3]`（默认 {0,0,0}）；`ShaderCompilerSlang::Compile` 从 linked layout 的 entry point 反射 `getComputeThreadGroupSize` 填充。`MetalComputePipeline` 从 `desc.cs->GetReflection()` 取出保存；encoder `Dispatch`/`DispatchIndirect` 用绑定 pipeline 的尺寸，缺失时回退 (1,1,1) 并记日志。Vulkan/DX12 忽略该字段（numthreads 内嵌于 SPIRV/DXIL）。

顺带：`CollectSlangResources` 对 `PushConstantBuffer` category 补记 `reflection.pushConstants`（SPIRV 路径 Vulkan push constant range 当前为空，一并修复）。

### D5: 深度/模板与光栅化状态挂在 pipeline 上，BindPipeline 时应用

`MetalGraphicsPipeline` 持有 `MTLDepthStencilState`（depthTest/depthWrite/compareOp + stencil front/back）与光栅化参数（cullMode/winding/fillMode/depthBias 三元组/depthClip/stencil reference）。`BindPipeline` 一次性设置。`MetalUtils.h` 补 `ToMetalStencilOp`。

### D6: BeginRendering 补 stencil attachment

depth-stencil 格式含 stencil（`GetImageFormatInfo().hasStencil`）时设置 `stencilAttachment`（texture 共用、load/store/clearStencil）；depth 部分同样改为按 `hasDepth` 门控。renderArea 不映射 scissor——Vulkan 也未这样做（renderArea 仅约束 render pass 区域）。

### D7: minConstantBufferAlignmentBytes → 常量 256

该 selector 不存在于 MTLDevice（编译错误）。macOS 常量 buffer 偏移对齐为 256，直接硬编码并注释原因。
