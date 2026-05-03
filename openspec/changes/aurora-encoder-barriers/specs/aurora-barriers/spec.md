## ADDED Requirements

### Requirement: BarrierInfo 聚合接口

接口层 SHALL 提供 `BarrierInfo` 结构体，聚合一次 barrier 调用所需的全部信息：

- `PipelineStageFlags srcStage`
- `PipelineStageFlags dstStage`
- `std::vector<MemoryBarrierInfo> memoryBarriers`
- `std::vector<BufferBarrierInfo> bufferBarriers`
- `std::vector<ImageBarrierInfo> imageBarriers`

`MemoryBarrierInfo` SHALL 包含 `AccessFlags srcAccess` 与 `AccessFlags dstAccess` 两个字段，用于表达全局可见性 barrier。

`ImageBarrierInfo` SHALL 包含 `oldLayout`（默认 `ImageLayout::UNDEFINED`）与 `newLayout`（必填）字段。

#### Scenario: 构造空 BarrierInfo
- **WHEN** 调用方构造 `BarrierInfo info{}` 并立即调用 `encoder->PipelineBarrier(info)`
- **THEN** 不崩溃；后端 MAY noop（无任何屏障要发出）

#### Scenario: ImageBarrierInfo 默认 oldLayout 是 UNDEFINED
- **WHEN** 调用方仅设置 `imageBarrier.image = img; imageBarrier.newLayout = COLOR_ATTACHMENT;`
- **THEN** `oldLayout` 默认值为 `UNDEFINED`，后端按 first-use 处理（不保留旧内容）

### Requirement: Encoder::PipelineBarrier 三类 Encoder 对称提供

`GraphicsEncoder`、`ComputeEncoder`、`BlitEncoder` SHALL 各自提供 `PipelineBarrier(const BarrierInfo &info)` 方法。

调用 `PipelineBarrier` 在 `BeginRendering` / `EndRendering` 区间**外**的合法性由后端按 native API 规则约束：
- Vulkan：`vkCmdPipelineBarrier2` 在 dynamic render pass 内合法（用于 self-dependency）
- DX12：`ResourceBarrier` 在 OMSetRenderTargets 之间合法
- Metal：encoder 内显式 barrier 仅 `memoryBarrierWithScope:` 可用，layout transition 在 encoder 之间隐式完成

#### Scenario: GraphicsEncoder 内 PipelineBarrier
- **WHEN** 在 `BeginRendering` 之前调用 `graphicsEncoder->PipelineBarrier(info)`，info 含一个 image transition `UNDEFINED → COLOR_ATTACHMENT`
- **THEN** 后续 `BeginRendering` 使用该 image 时，validation layer / debug layer 不报 layout 警告

#### Scenario: ComputeEncoder 内 PipelineBarrier
- **WHEN** 在 `Dispatch` 之间插入 `PipelineBarrier`，srcStage=COMPUTE_SHADER、dstStage=COMPUTE_SHADER、memoryBarrier 含 `srcAccess=COMPUTE_UAV_WRITE, dstAccess=COMPUTE_UAV_READ`
- **THEN** 第二个 Dispatch 能正确读取第一个 Dispatch 的 UAV 写入结果

#### Scenario: BlitEncoder 内 PipelineBarrier
- **WHEN** 在 CopyBufferToImage 之后、ShaderRead 之前，BlitEncoder 内 `PipelineBarrier` 把 image 从 `TRANSFER_DST → SHADER_READ_ONLY`
- **THEN** 后续 graphics 采样该 image 不报 layout 不匹配

### Requirement: AccessFlags ↔ PipelineStage ↔ Layout 转换语义

后端实现 SHALL 把 `AccessFlags`、`PipelineStageFlags`、`ImageLayout` 按以下原则翻译：

- Vulkan：直接位映射到 `VkAccessFlags2` / `VkPipelineStageFlags2` / `VkImageLayout`
- DX12：聚合 access 推导 `D3D12_RESOURCE_STATES`（transition before/after）；UAV write → UAV barrier；layout 概念 noop
- Metal：access → `MTLBarrierScope` + `MTLRenderStages`；layout 概念 noop
- GLES：access → `glMemoryBarrier` 位（最大颗粒度，可拍成单次 `glMemoryBarrier`）；layout / stage 忽略

后端 MUST 在 debug build 下校验 `AccessFlags` 与 `oldLayout`/`newLayout` 的一致性（如 `dstAccess=SHADER_READ` 但 `newLayout=COLOR_ATTACHMENT` 应触发 assert）。

#### Scenario: COLOR_WRITE → SHADER_READ 转换
- **WHEN** 调用方在 EndRendering 之后插入 image barrier：`srcAccess=COLOR_WRITE, dstAccess=FRAGMENT_SRV, oldLayout=COLOR_ATTACHMENT, newLayout=SHADER_READ_ONLY, srcStage=COLOR_OUTPUT, dstStage=FRAGMENT_SHADER`
- **THEN** 后续在另一 pass 中采样该 image 不报错；GPU readback 验证内容正确

#### Scenario: TRANSFER_WRITE → COLOR_WRITE 转换
- **WHEN** 调用方在 CopyBufferToImage 之后做 BeginRendering 用同一 image 作为 RT，barrier `srcAccess=TRANSFER_WRITE, dstAccess=COLOR_WRITE, oldLayout=TRANSFER_DST, newLayout=COLOR_ATTACHMENT, srcStage=TRANSFER, dstStage=COLOR_OUTPUT`
- **THEN** 渲染输出包含 transfer 写入的初始内容（LoadOp::LOAD 时）

### Requirement: InferLayoutForAccess 工具函数

接口层 SHALL 提供自由函数 `ImageLayout InferLayoutForAccess(AccessFlags access)`，按以下规则映射：

- access 仅含 `COLOR_WRITE` / `COLOR_INOUT_WRITE` → `COLOR_ATTACHMENT`
- access 仅含 `DEPTH_STENCIL_WRITE` / `DEPTH_STENCIL_INOUT_WRITE` → `DEPTH_STENCIL_ATTACHMENT`
- access 仅含 `DEPTH_STENCIL_READ` → `DEPTH_STENCIL_READ_ONLY`
- access 仅含任意 stage 的 `*_SRV` → `SHADER_READ_ONLY`
- access 含任意 `*_UAV_*` → `GENERAL`
- access 仅含 `TRANSFER_READ` → `TRANSFER_SRC`
- access 仅含 `TRANSFER_WRITE` → `TRANSFER_DST`
- access 仅含 `PRESENT` → `PRESENT`
- access 含多类冲突 → `GENERAL`
- access 为空 (`NONE`) → `UNDEFINED`

调用方 MAY 用此函数减少手写 layout 的负担：

```cpp
ImageBarrierInfo b{};
b.image = img;
b.srcAccess = COLOR_WRITE;
b.dstAccess = FRAGMENT_SRV;
b.oldLayout = InferLayoutForAccess(b.srcAccess);
b.newLayout = InferLayoutForAccess(b.dstAccess);
```

#### Scenario: 单 access 推导
- **WHEN** 调用 `InferLayoutForAccess(COLOR_WRITE)`
- **THEN** 返回 `COLOR_ATTACHMENT`

#### Scenario: 跨 stage 同语义 SRV
- **WHEN** 调用 `InferLayoutForAccess(VERTEX_SRV | FRAGMENT_SRV)`
- **THEN** 返回 `SHADER_READ_ONLY`（多 stage 同 SRV 仍合并为 SHADER_READ_ONLY）

#### Scenario: 冲突 access 退化到 GENERAL
- **WHEN** 调用 `InferLayoutForAccess(COLOR_WRITE | FRAGMENT_SRV)`
- **THEN** 返回 `GENERAL`（写 + 读到不同 layout，无法收敛）

#### Scenario: 空 access
- **WHEN** 调用 `InferLayoutForAccess(AccessFlagBit::NONE)`
- **THEN** 返回 `UNDEFINED`

### Requirement: Acquire / Present 的 layout transition 由调用方显式发起

aurora 后端在 `SwapChain::AcquireNextImage` 与 `SwapChain::Present` **不**插入隐式 barrier。调用方 MUST 在录制 cmdbuf 时显式：

- 渲染前：从 `UNDEFINED`（或 `PRESENT`）transition 到 `COLOR_ATTACHMENT`
- 渲染后：从 `COLOR_ATTACHMENT` transition 到 `PRESENT`

#### Scenario: 完整 present 帧的 layout 链
- **WHEN** 调用方按以下顺序执行：Acquire → 录制 cmdbuf 含 `barrier(UNDEFINED→COLOR_ATTACHMENT)` + BeginRendering + Draw + EndRendering + `barrier(COLOR_ATTACHMENT→PRESENT)` → Submit → Present
- **THEN** Present 不报 layout 错误；下一帧 Acquire 仍能拿到该 image

#### Scenario: 缺少 transition 触发 validation 警告
- **WHEN** 调用方录制不含任何 image barrier 的 cmdbuf 直接渲染 swapchain image 后 Present
- **THEN** Vulkan validation layer / D3D12 debug layer 报告 layout 不匹配（说明 barrier 责任在调用方）
