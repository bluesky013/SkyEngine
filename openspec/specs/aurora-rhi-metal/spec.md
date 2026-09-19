# aurora-rhi-metal Specification

## Purpose
Aurora RHI Metal 后端：slang MSL 直发路径下的 shader 加载、直接绑定 ResourceGroup、深度模板/光栅化状态、compute dispatch 与 push constant 约定。

## Requirements

### Requirement: Metal shader function 按 entry 名加载

slang MSL 输出保留源码级 entry 名（如 `mainVS`）。`ShaderFunction::Descriptor.entry` SHALL 传递给 Metal 后端并用于 `newFunctionWithName`；entry 为空时 Metal SHALL 回退 `VSMain/FSMain/CSMain` 约定（兼容手写 MSL）。

#### Scenario: slang MSL shader 加载

- **WHEN** 用 slang MSL 编译产物（entry `mainVS`/`mainFS`/`mainCS`）创建 `ShaderFunction`
- **THEN** `newFunctionWithName` 命中，`AuroraMetalTest.SlangToRhiShaderObjects` / `SlangToRhiComputePipeline` 通过

#### Scenario: 手写 MSL 回退

- **WHEN** `desc.entry` 为空且 MSL 源码入口为 `VSMain/FSMain/CSMain`
- **THEN** shader function 创建成功

### Requirement: Metal ResourceGroup 直接绑定模型

slang Metal target SHALL 被视为扁平绑定模型：资源按类别（buffer/texture/sampler）各自连续编号（`[[buffer(N)]]`/`[[texture(N)]]`/`[[sampler(N)]]`)，`[[vk::binding]]` 的 space 被忽略，MSL 反射的 `binding` 即类别内索引。Metal ResourceGroup SHALL 以 binding→资源表记录写入，并在 `BindResourceGroup` 时以 `setBuffer/setTexture/setSampler` 直接应用（无 argument buffer)。`DescriptorEncoder`/`DescriptorBatch` SHALL 立即写入 group 表（无批量原生对象）。`CreateResourceGroup`/`CreateDescriptorBatch` SHALL NOT 返回 nullptr。

#### Scenario: ResourceGroup 创建与写入

- **WHEN** pipeline 层以 `{shader, set}` 创建 ResourceGroup 并经 encoder/batch 写入 buffer/image/sampler
- **THEN** 对象创建成功，写入记录进绑定表（`set` 与 `ImageLayout` 在 Metal 上忽略）

#### Scenario: 绑定应用

- **WHEN** 图形/计算 encoder `BindResourceGroup`
- **THEN** group 内所有 buffer/texture/sampler 被设置到对应类别的 binding 槽位（图形同时设置 vertex 与 fragment 阶段）

### Requirement: Metal 深度模板与光栅化状态

`MetalGraphicsPipeline` SHALL 从 `PipelineState::depthStencil` 创建 `MTLDepthStencilState`（depthTest/depthWrite/compareOp/stencil front/back)，并保存光栅化参数（cullMode/frontFace/polygonMode/depthBias/depthClamp/stencil reference);`BindPipeline` SHALL 将上述状态应用到 render encoder。`BeginRendering` SHALL 按格式 `hasDepth`/`hasStencil` 门控 depth/stencil attachment（含 load/store/clearStencil)。

#### Scenario: 深度测试生效

- **WHEN** pipeline state 声明 `depthTest=true, compareOp=LESS_OR_EQUAL` 且绑定该 pipeline
- **THEN** encoder 应用对应 `MTLDepthStencilState` 与 cull/winding/fill/depthBias/depthClip 设置

#### Scenario: stencil attachment 完整

- **WHEN** `BeginRendering` 的 depth-stencil 图像格式含 stencil
- **THEN** `stencilAttachment` 的 texture/loadAction/storeAction/clearStencil 被设置

### Requirement: Metal compute dispatch 使用反射线程组尺寸

MSL 不携带 `numthreads`。`ShaderReflection` SHALL 携带 `threadGroupSize[3]`，由 slang entry point 反射 `getComputeThreadGroupSize` 填充；`MetalComputePipeline` SHALL 保存该尺寸，`Dispatch`/`DispatchIndirect` SHALL 使用绑定 pipeline 的尺寸而非硬编码 (1,1,1)。

#### Scenario: 非 (1,1,1) kernel 正确 dispatch

- **WHEN** compute shader 声明 `[numthreads(8,4,2)]` 并经 slang MSL 编译
- **THEN** `Dispatch` 的 `threadsPerThreadgroup` 为 (8,4,2)

### Requirement: Metal push constant 槽位约定

slang 在 Metal 上把 `[[vk::push_constant]]` 物化为普通 `constant T* [[buffer(N)]]`。引擎约定 push constant 块在 shader 中最后声明；`MetalShader` SHALL 将 push constant 槽位推导为 buffer 类资源的最高 binding + 0（即资源数 - 1）,encoder `PushConstants` SHALL 用 `setVertexBytes/setFragmentBytes/setBytes`（按 stageFlags）写入该槽位。

#### Scenario: push constant 写入

- **WHEN** shader 声明 push constant 块（最后声明）且 encoder 调 `PushConstants`
- **THEN** 数据经 `set*Bytes` 写入推导槽位，不与普通 buffer 资源冲突

### Requirement: shader 编译器反射补全

`ShaderCompilerSlang` SHALL 对 `PushConstantBuffer` category 记录 `reflection.pushConstants`（不再产生伪 UBO descriptor 资源），SHALL 在 compute 编译时填充 `threadGroupSize`。

#### Scenario: SPIRV push constant range

- **WHEN** SPIRV 目标编译含 `[[vk::push_constant]]` 的 shader
- **THEN** `reflection.pushConstants` 含对应 range，且 resources 中无对应伪 UBO
