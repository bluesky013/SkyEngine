## 1. 接口与 shader 编译器

- [x] 1.1 `ShaderFunction::Descriptor` 增加 `entry`（std::string，默认空）；`ShaderReflection` 增加 `threadGroupSize[3]`
- [x] 1.2 `ShaderCompilerSlang`：填充 `threadGroupSize`（compute entry 反射）；`PushConstantBuffer` category 补记 `reflection.pushConstants`
- [x] 1.3 测试 helper `MakeShaderFuncDesc` 传 entry；`RgBlockDesc.h` 注释更新为直接绑定模型

## 2. Metal shader / pipeline

- [x] 2.1 `MetalShaderFunction`：用 `desc.entry` 查函数，空回退 VSMain/FSMain/CSMain
- [x] 2.2 `MetalShader`：保存 reflection；统计 buffer 类资源数推导 pushConstantSlot；specialization 快照对齐 Vulkan
- [x] 2.3 `MetalGraphicsPipeline`：MTLDepthStencilState + 光栅化参数（cull/winding/fill/depthBias/depthClip/stencil ref）
- [x] 2.4 `MetalComputePipeline`：保存 threadGroupSize
- [x] 2.5 `MetalDevice::UpdateDeviceCaps`：修复 minConstantBufferAlignmentBytes 编译错误（常量 256）

## 3. Metal encoder

- [x] 3.1 `BeginRendering`：stencil attachment（depth/stencil 按 hasDepth/hasStencil 门控）
- [x] 3.2 `BindPipeline`：应用深度模板与光栅化状态
- [x] 3.3 `PushConstants`：set*Bytes 到 pushConstantSlot（图形按 stageFlags；计算 setBytes）
- [x] 3.4 `Draw*`：使用 pipeline topology
- [x] 3.5 `Dispatch`/`DispatchIndirect`：使用 pipeline threadGroupSize
- [x] 3.6 `BindResourceGroup`：应用 group 三张绑定表

## 4. Metal ResourceGroup 栈

- [x] 4.1 新增 `MetalResourceGroup`（binding→buffer/texture/sampler 三张表，CounterPtr 持有资源）
- [x] 4.2 `MetalDescriptorEncoder`/`MetalDescriptorBatch` 改为真实实现（立即写入 group 表）
- [x] 4.3 `MetalDevice::CreateResourceGroup`/`CreateDescriptorBatch` 返回真实对象

## 5. 验证

- [x] 5.1 macOS 编译引擎（metal rhi + shader 模块）通过
- [x] 5.2 `AuroraMetalTest.SlangToRhiShaderObjects` / `SlangToRhiComputePipeline` 通过
