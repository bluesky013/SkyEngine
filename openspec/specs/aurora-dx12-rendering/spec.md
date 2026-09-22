# aurora-dx12-rendering Specification

## Purpose
TBD - created by archiving change aurora-dx12-gaps. Update Purpose after archive.
## Requirements
### Requirement: BeginRendering 绑定并清理 render attachment

`D3D12GraphicsEncoder::BeginRendering(const RenderingInfo &)` SHALL 为 `numColors` 个颜色附件各建一个 RTV、为 depth/stencil 附件建 DSV，并调用 `OMSetRenderTargets(numColors, rtvs, FALSE, dsv)`；SHALL 在 `LoadOp::CLEAR` 时用 `RenderingInfo::clearValue` 调 `ClearRenderTargetView` / `ClearDepthStencilView`。`D3D12Image` SHALL 提供 `CreateRTV` / `CreateDSV`（按 subresource range）。RTV/DSV descriptor SHALL 来自 per-frame ring 分配，且 SHALL NOT 被其它 in-flight frame 覆盖。

#### Scenario: 绑定并清理颜色与深度

- **WHEN** 传入 1 个 color attachment（`LoadOp::CLEAR`）+ 1 个 depth attachment（`LoadOp::CLEAR`）
- **THEN** 该 pass 的 draw 输出到对应 color image；depth 写入对应 depth image；clear 值生效

#### Scenario: LoadOp DONT_CARE 不清理

- **WHEN** attachment 的 `LoadOp` 为 `DONT_CARE`
- **THEN** SHALL NOT 发对应 clear 调用，内容保留

#### Scenario: in-flight frame 不互相覆盖

- **WHEN** 连续两帧（不同 frame slot）各自 `BeginRendering`
- **THEN** 两帧的 RTV/DSV descriptor 互不覆盖

### Requirement: Compute encoder 使用 compute root 参数

`D3D12ComputeEncoder::BindResourceGroup` SHALL 用 `SetComputeRootDescriptorTable` / `SetComputeRootConstantBufferView` / `SetComputeRootUnorderedAccessView`，`PushConstants` SHALL 用 `SetComputeRoot32BitConstants`；SHALL NOT 在 compute command list 上调用 `SetGraphicsRoot*`。

#### Scenario: compute 绑定 descriptor table

- **WHEN** 在 compute encoder 上 `BindResourceGroup(set, group)`
- **THEN** 调用的是 compute root 变体，dispatch 能读到绑定资源

#### Scenario: compute push constants

- **WHEN** 在 compute encoder 上 `PushConstants(offset, size, data)`
- **THEN** 调用的是 `SetComputeRoot32BitConstants`，shader 读到正确值

### Requirement: 顶点输入装配

`D3D12GraphicsPipeline::Init` SHALL 从 `PipelineState::vertexBindings` / `vertexAttributes` 构造真实 `D3D12_INPUT_LAYOUT_DESC` 并设置 `InputLayout.NumElements`。`D3D12GraphicsEncoder::BindPipeline` SHALL 缓存该 PSO 的 per-binding stride；`BindVertexBuffers` SHALL 用缓存的 stride 填 `StrideInBytes`，SHALL NOT 恒为 0。

#### Scenario: 输入布局被创建

- **WHEN** `PipelineState` 声明 1 个 binding（stride 32）+ 2 个 attribute
- **THEN** PSO 的 `InputLayout.NumElements == 2`，semantic/format/offset 与声明一致

#### Scenario: 绑定使用 PSO stride

- **WHEN** `BindPipeline(pso)` 后 `BindVertexBuffers(0, 1, &view)`
- **THEN** 提交的 `StrideInBytes == pso` 对应 binding 的 stride

### Requirement: Index buffer size 由 buffer 大小推导

`Buffer` SHALL 暴露大小（`GetSize()`），`D3D12Buffer` SHALL 返回其分配大小。`D3D12GraphicsEncoder::BindIndexBuffer` SHALL 以 `size - offset` 填 `SizeInBytes`，SHALL NOT 恒为 0。

#### Scenario: index buffer 视图大小正确

- **WHEN** 对大小为 N、offset 为 O 的 index buffer 调 `BindIndexBuffer`
- **THEN** `D3D12_INDEX_BUFFER_VIEW.SizeInBytes == N - O`

### Requirement: Indirect draw 与 dispatch

D3D12 backend SHALL 为 draw / draw-indexed / dispatch 各持一个可复用的 `ID3D12CommandSignature`。`DrawIndirect` / `DrawIndexedIndirect` SHALL 用 `ExecuteIndirect` 按 `drawCount` 执行，`DispatchIndirect` SHALL 用 dispatch 签名执行。

#### Scenario: indexed indirect draw

- **WHEN** 参数 buffer 内含 3 个 `D3D12_DRAW_INDEXED_ARGUMENTS`，调 `DrawIndexedIndirect(buffer, 0, 3, stride)`
- **THEN** `ExecuteIndirect` 以 `MaxCommandCount == 3` 执行对应签名

#### Scenario: dispatch indirect

- **WHEN** 调 `DispatchIndirect(buffer, offset)`
- **THEN** 以 dispatch 签名执行一次 `ExecuteIndirect`

### Requirement: BlitImage 支持 copy 与缩放/过滤

`D3D12BlitEncoder::BlitImage` SHALL 在 region 的 src/dst extent 相等且不需要过滤时用 `CopyTextureRegion`；否则 SHALL 用内置 fullscreen blit pipeline（内嵌预编译 DXIL）把 src 渲染到 dst（dst 作 RTV）。SHALL NOT 为空实现。

#### Scenario: 同尺寸 blit 走 copy

- **WHEN** region 的 src/dst extent 相等、`Filter::NEAREST`
- **THEN** 走 `CopyTextureRegion`，dst 内容与 src 一致

#### Scenario: 缩放 blit 走内置管线

- **WHEN** region 的 src extent 小于 dst extent
- **THEN** 用内置 fullscreen blit PSO 渲染，dst 为放大后的图像

### Requirement: ResolveImage subresource 索引正确

`D3D12BlitEncoder::ResolveImage` SHALL 用 `level + baseLayer * mipLevels` 计算 src/dst subresource 索引（mipLevels 取各自 image），SHALL NOT 用固定 `* 1`。

#### Scenario: 多 mip 图像 resolve

- **WHEN** 对 mipLevels > 1 的图像执行 resolve，region 指向 mip > 0
- **THEN** 解析到正确 subresource，而非固定按 mip 数 1 计算

### Requirement: PushConstants 4 字节对齐校验

`D3D12GraphicsEncoder::PushConstants` 与 `D3D12ComputeEncoder::PushConstants` SHALL 在校验 `offset % 4 == 0 && size % 4 == 0` 后才调用 `Set{Graphics,Compute}Root32BitConstants`；非对齐 SHALL 记录错误并返回，SHALL NOT 静默截断。

#### Scenario: 非对齐 push 被拒绝

- **WHEN** `PushConstants(offset=2, size=6, data)`
- **THEN** 记录错误并返回，不调用 root constant 设置

### Requirement: VertexBufferView range 回退

`D3D12GraphicsEncoder::BindVertexBuffers` SHALL 在 `BufferView::range == 0` 时用 `buffer->GetSize() - offset` 作为 `SizeInBytes`，并 SHALL 跳过空 buffer。

#### Scenario: range 为零时用整缓冲

- **WHEN** `BindVertexBuffers` 收到 `range == 0` 的 view
- **THEN** `SizeInBytes` 为 `GetSize() - offset`，不是 0

### Requirement: DX12 调试层 break 策略

DX12 后端在 debug 构建启用 info queue 时，SHALL 仅在附加调试器的情况下对 ERROR / CORRUPTION 级别消息 break；
未附加调试器时 SHALL NOT 终止进程（消息仍被记录，不静默吞掉）。

#### Scenario: 无调试器不致命
- **WHEN** debug 构建下未附加调试器运行，且出现 error 级 DX12 消息
- **THEN** 进程 SHALL 继续运行，而不是在该消息处 break 或退出

#### Scenario: 有调试器保留 break
- **WHEN** 已附加调试器且出现 error 级 DX12 消息
- **THEN** 后端 SHALL 在该消息处 break，便于定位

