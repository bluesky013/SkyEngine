# aurora-barriers Specification

## Purpose
TBD - created by archiving change aurora-encoder-barriers. Update Purpose after archive.
## Requirements
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

### Requirement: CommandBuffer::PipelineBarrier 单一入口

`CommandBuffer` SHALL 提供 `PipelineBarrier(const BarrierInfo &info)` 方法。Encoder 接口（GraphicsEncoder / ComputeEncoder / BlitEncoder）**不**暴露 PipelineBarrier。

调用方 MAY 在以下任意时机调用：
- 任何 Encoder 创建之前（典型：pass 之间 transition）
- 一个 Encoder 已创建但未做实质工作时
- 一个 Encoder 已 Encode 部分命令、还在 record 中时（典型：compute UAV→UAV barrier 在两次 Dispatch 之间）
- `BeginRendering` / `EndRendering` 区间**内**仅当 Vulkan 后端 self-dependency 合法（一般不推荐）

后端实现职责：
- Vulkan / DX12 / GLES：直接落到 cmdbuf-级 native API
- Metal：CommandBuffer 内部记账当前 active encoder；若有，转 `[encoder memoryBarrierWithScope:after:before:]`；若无（pass 之间），缓存到下一次 `CreateXxxEncoder` 入口处 flush

#### Scenario: 在 Encoder 创建前发 image transition
- **WHEN** 调用方依次：`cmdBuf->Begin()` → `cmdBuf->PipelineBarrier(info_undefined_to_color)` → `cmdBuf->CreateGraphicsEncoder()` → `BeginRendering(...)`
- **THEN** validation layer / debug layer 不报 layout 警告；Metal 后端把 barrier 在新 encoder 入口处 flush

#### Scenario: Compute 序列中间 UAV→UAV barrier
- **WHEN** 在 `Dispatch` 之间调用 `cmdBuf->PipelineBarrier(info)`，info 含 srcStage=COMPUTE_SHADER、dstStage=COMPUTE_SHADER、memoryBarrier 含 `srcAccess=COMPUTE_UAV_WRITE, dstAccess=COMPUTE_UAV_READ`
- **THEN** 第二个 Dispatch 能正确读取第一个 Dispatch 的 UAV 写入结果；Metal 后端把 barrier 路由到当前 active 的 ComputeEncoder

#### Scenario: Blit 之前的 transition
- **WHEN** 在 `CreateBlitEncoder` 之前调用 `cmdBuf->PipelineBarrier(...)` 把 image 从 `SHADER_READ_ONLY → TRANSFER_DST`，然后 BlitEncoder 内做 CopyBufferToImage
- **THEN** copy 不报 layout 不匹配

#### Scenario: 跨 Encoder 边界
- **WHEN** EndRendering → `cmdBuf->PipelineBarrier(color_to_shader_read)` → CreateGraphicsEncoder/BeginRendering 用同一 image 作为 SRV
- **THEN** 第二个 pass 采样正确；validation 不报错

### Requirement: AccessFlags ↔ PipelineStage ↔ Layout 转换语义

`AccessFlags` SHALL 表达 **stage-agnostic 的资源访问类别**，不再编码 shader stage：

- `SRV`（shader 只读，合并所有 `*_SRV`）
- `UAV`（shader 读写 / storage，合并所有 `*_UAV_READ` / `*_UAV_WRITE`）
- `CBV`（constant buffer，合并所有 `*_CBV`）
- `RTV`（render target 写，合并 `COLOR_WRITE` / `COLOR_INOUT_WRITE` / `COLOR_READ` / `COLOR_INPUT`）
- `DSV`（depth/stencil 写，合并 `DEPTH_STENCIL_WRITE` / `DEPTH_STENCIL_INOUT_WRITE`）
- `DSV_READ`（depth/stencil 读，合并 `DEPTH_STENCIL_READ` / `DEPTH_STENCIL_INOUT_READ` / `DEPTH_STENCIL_INPUT`）
- `COPY_SRC`（`TRANSFER_READ`）、`COPY_DST`（`TRANSFER_WRITE`）
- `PRESENT`、`VERTEX_BUFFER`、`INDEX_BUFFER`、`INDIRECT_BUFFER`、`GENERAL`、`NONE`

stage 由调用方在 `BarrierInfo.srcStage` / `dstStage` **显式指定**（RDG 按 pass 类型推断），不再从 access 反解。后端实现 SHALL 把 `AccessFlags`、`PipelineStageFlags`、`ImageLayout` 按以下原则翻译：

- Vulkan：access 类别直接映射 `VkAccessFlags2`（`SRV`→`SHADER_READ`，`UAV`→`SHADER_WRITE`，`RTV`→`COLOR_ATTACHMENT_WRITE`，…）；stage 由 `srcStage`/`dstStage` 直接映射 `VkPipelineStageFlags2`
- DX12：聚合 access 推导 `D3D12_RESOURCE_STATES`（transition before/after）；UAV → UAV barrier；layout 概念 noop
- Metal：access → `MTLBarrierScope` + `MTLRenderStages`；layout 概念 noop
- GLES：access → `glMemoryBarrier` 位；layout / stage 忽略

后端 MUST 在 debug build 下校验 `AccessFlags` 与 `oldLayout`/`newLayout` 的一致性。

#### Scenario: RTV → SRV 转换
- **WHEN** 调用方在 EndRendering 之后插入 image barrier：`srcAccess=RTV, dstAccess=SRV, oldLayout=COLOR_ATTACHMENT, newLayout=SHADER_READ_ONLY, srcStage=COLOR_OUTPUT, dstStage=FRAGMENT_SHADER`
- **THEN** 后续在另一 pass 采样该 image 不报错；GPU readback 验证内容正确

#### Scenario: COPY_DST → RTV 转换
- **WHEN** 调用方在 CopyBufferToImage 之后 BeginRendering 用同一 image 作为 RT，barrier `srcAccess=COPY_DST, dstAccess=RTV, oldLayout=TRANSFER_DST, newLayout=COLOR_ATTACHMENT, srcStage=TRANSFER, dstStage=COLOR_OUTPUT`
- **THEN** 渲染输出包含 transfer 写入的初始内容（LoadOp::LOAD 时）

### Requirement: InferLayoutForAccess 工具函数

接口层 SHALL 提供自由函数 `ImageLayout InferLayoutForAccess(AccessFlags access)`，按以下规则映射（access 为 stage-agnostic 类别）：

- access 仅含 `RTV` → `COLOR_ATTACHMENT`
- access 仅含 `DSV` → `DEPTH_STENCIL_ATTACHMENT`
- access 仅含 `DSV_READ` → `DEPTH_STENCIL_READ_ONLY`
- access 仅含 `SRV` → `SHADER_READ_ONLY`
- access 含 `UAV` → `GENERAL`
- access 仅含 `COPY_SRC` → `TRANSFER_SRC`
- access 仅含 `COPY_DST` → `TRANSFER_DST`
- access 仅含 `PRESENT` → `PRESENT`
- access 含多类冲突 → `GENERAL`
- access 为空 (`NONE`) → `UNDEFINED`

调用方 MAY 用此函数减少手写 layout 的负担：

```cpp
ImageBarrierInfo b{};
b.image = img;
b.srcAccess = RTV;
b.dstAccess = SRV;
b.oldLayout = InferLayoutForAccess(b.srcAccess);
b.newLayout = InferLayoutForAccess(b.dstAccess);
```

#### Scenario: 单 access 推导
- **WHEN** 调用 `InferLayoutForAccess(RTV)`
- **THEN** 返回 `COLOR_ATTACHMENT`

#### Scenario: SRV 推导
- **WHEN** 调用 `InferLayoutForAccess(SRV)`
- **THEN** 返回 `SHADER_READ_ONLY`

#### Scenario: 冲突 access 退化到 GENERAL
- **WHEN** 调用 `InferLayoutForAccess(RTV | SRV)`
- **THEN** 返回 `GENERAL`（写 + 读收敛不到单一 layout）

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

