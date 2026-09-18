## 1. 共享接口与 executor 基础（先落地，保证三后端可编译）

- [x] 1.1 `aurora/rhi/Core.h`：`PipelineState` 增加 `vertexBindings` / `vertexAttributes`；`VertexAttributeDesc` 的 `sematic` 改名 `semantic` 并新增 `semanticIndex`
- [x] 1.2 `aurora/rhi/Buffer.h`：新增 `virtual uint64_t GetSize() const { return 0; }`
- [x] 1.3 全仓 grep 并修正 `sematic` 引用（Vulkan/Metal/DX12/tests）——aurora 侧无引用，旧 `engine/render` 的 `sematic` 为无关的 semantic flag，未动
- [x] 1.4 `aurora/rhi/interface/src/rdg/Execute.cpp`：SCENE_RASTER 在 `BindIndexBuffer`/`DrawIndexed` 前对 `item.vb != nullptr` 调 `BindVertexBuffers(0, 1, &view)`
- [x] 1.5 编译 Vulkan/DX12 后端 + `AuroraRHITest` 通过（Metal 在 Windows 上不可编译）

## 2. DX12 encoder 正确性（解锁可见渲染）

- [x] 2.1 `D3D12ComputeEncoder::BindResourceGroup` 改用 `SetComputeRootDescriptorTable` / `SetComputeRootConstantBufferView` / `SetComputeRootUnorderedAccessView`
- [x] 2.2 `D3D12ComputeEncoder::PushConstants` 改用 `SetComputeRoot32BitConstants`
- [x] 2.3 `D3D12Buffer` 覆写 `GetSize()`，存储 descriptor size
- [x] 2.4 `D3D12GraphicsEncoder::BindIndexBuffer` 用 `GetSize() - offset` 填 `SizeInBytes`
- [x] 2.5 `D3D12DescriptorAllocator`：新增 RTV / DSV per-frame ring（`ringSize` 份）+ `mCurrentFrame` bump 分配/重置
- [x] 2.6 `D3D12Image`：新增 `CreateRTV(handle, subRange)` / `CreateDSV(handle, subRange)`
- [x] 2.7 `D3D12GraphicsEncoder::BeginRendering`：分配 RTV/DSV、建视图、`OMSetRenderTargets`、按 `LoadOp::CLEAR` clear
- [x] 2.8 验证：`EncoderTestD3D12.GraphicsEncoderRenderPass` 走 color+depth `BeginRendering`（clear）并通过（debug layer break-on-error 下无 validation 报错）

## 3. 输入装配（IA）

- [x] 3.1 `D3D12GraphicsPipeline::Init`：从 `PipelineState` 顶点布局构造 `D3D12_INPUT_LAYOUT_DESC`（semantic/semanticIndex/format/offset/inputSlot + inputSlotClass）
- [x] 3.2 `D3D12GraphicsPipeline` 暴露每 binding 的 stride（`GetVertexStrides()` 或等价）
- [x] 3.3 `D3D12GraphicsEncoder::BindPipeline` 缓存 stride，`BindVertexBuffers` 使用缓存 stride

## 4. Indirect draw / dispatch

- [x] 4.1 `D3D12Device` 新增 command signature 缓存（key = indirect kind + stride），懒创建 draw/draw-indexed/dispatch 三种签名
- [x] 4.2 `D3D12GraphicsEncoder::DrawIndirect` / `DrawIndexedIndirect` 用 `ExecuteIndirect`（`MaxCommandCount = drawCount`）
- [x] 4.3 `D3D12ComputeEncoder::DispatchIndirect` 用 dispatch 签名 `ExecuteIndirect`

## 5. BlitImage

- [x] 5.1 生成内置 fullscreen blit 的预编译 DXIL 并作为头文件内嵌（`dx12/gen/`），附 HLSL 源与再生成命令（`D3D12Blit.hlsl` + `regen_blit_shader.ps1` + `D3D12BlitShader.h`）
- [x] 5.2 `D3D12BlitHelper`：root signature + sampler + fullscreen PSO，渲染 src → dst(RTV)
- [x] 5.3 `D3D12BlitEncoder::BlitImage`：同 extent 走 `CopyTextureRegion`，否则走 helper；RDG `CopyBlitPassBuilder` 新增 `Src/Dst(handle, access)` 变体以声明 SRV/RTV
- [x] 5.4 验证：`EncoderTestD3D12.BlitImageSameSizeCopy`（CopyTextureRegion）与 `BlitImageScaled`（内置管线）通过

## 6. tier2 bindless descriptor heap

- [x] 6.1 `D3D12DescriptorHeap : DescriptorHeap`：resource + sampler shader-visible backing heap，per-type free-list 分配/释放
- [x] 6.2 即时写入路径：`CreateHeapEncoder(allocation)` 返回 heap-bound `DescriptorEncoder`，`Write*` 立即写 descriptor；基类 `Update` 为 no-op（D3D12 无批量 flush）
- [x] 6.3 `D3D12Device::CreateDescriptorHeap` 返回实例（能力门开启时）
- [x] 6.4 `UpdateDeviceCaps` 用 `D3D12_FEATURE_SHADER_MODEL >= 6.6` 记录能力并 gate
- [x] 6.5 `BindDescriptorHeap`（graphics + compute）用 `SetDescriptorHeaps` 绑定 heap 对
- [x] 6.7 验证：`EncoderTestD3D12.DescriptorHeapAllocateFree` 覆盖分配/释放（SM6.6 设备；低版本走 `GTEST_SKIP`）

## 7. 测试与文档

- [x] 7.1 `AuroraRHITest` 增加 DX12 用例：render-target+clear / blit（copy + scaled）/ descriptor heap（IA indexed draw 与 indirect 用例见 Deferred）
- [x] 7.2 `AuroraShaderTest` D3D12 仍绿（40 tests passed）
- [x] 7.3 修正 `engine/aurora/AGENTS.md`「DX12 PSO 仍是 stub」及能力说明
- [x] 7.4 修正 `engine/aurora/shader/test/SlangD3D12Test.cpp` 过时注释
- [x] 7.5 `openspec validate aurora-dx12-gaps --strict` 通过

## 8. DX12/VK 对齐修正（P0 + P1）

- [x] 8.1 P0：`D3D12BlitEncoder::ResolveImage` subresource 索引用 `level + baseLayer * mipLevels`（原来 `* 1`）
- [x] 8.2 P0：`D3D12GraphicsEncoder/ComputeEncoder::PushConstants` 校验 4 字节对齐，非对齐报错而非静默截断
- [x] 8.3 P0：`D3D12GraphicsEncoder::BindVertexBuffers` 在 `range == 0` 时回退 `GetSize() - offset`，并加空 buffer 保护、按约定用 `SKY_ASSERT` 检查 `MAX_VERTEX_BINDINGS`
- [x] 8.4 P1：Vulkan PSO 从 `PipelineState::vertexBindings/vertexAttributes` 构造 `VkPipelineVertexInputStateCreateInfo`（IA 与 DX12 对齐）
- [x] 8.5 P1：`Device` 暴露 `DeviceFeature`（`GetFeature()`）；DX12 `UpdateDeviceCaps` 置 `feature.descriptorHeap = SM6.6`，`CreateDescriptorHeap` 据此 gate；Vulkan 显式置 false（tier2 未实现）
- [x] 8.6 P1：确认 RDG barrier 用 `FullSubRange`（全 mip/layer），DX12 `ALL_SUBRESOURCES` 与 VK full-range 语义一致，无需改
- [x] 8.7 回归：`AuroraRHITest` 130 passed；`AuroraShaderTest` 40 passed

## Deferred（明确不在本 change 范围，留待后续）

- DX12 IA indexed draw 的真实出图验证（需要顶点属性 shader + readback）。
- DX12 indirect draw/dispatch 的参数 buffer 单测（间接路径当前无 pass 驱动）。
- tier2 heap 的 `HEAP_WITH_PUSH_INDEX` 逐 draw 索引映射：需要 slang 侧 `ResourceDescriptorHeap` 索引来源的 shader/绑定约定，未定义。
