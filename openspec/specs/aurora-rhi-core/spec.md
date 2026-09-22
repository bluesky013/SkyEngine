# aurora-rhi-core Specification

## Purpose
TBD - consolidated from: aurora-rhi-conventions aurora-sync-primitives aurora-barriers aurora-queue aurora-swapchain aurora-frame-context aurora-frame-dispatcher aurora-frame-descriptor-batch
## Requirements
### Requirement: Sampler 默认值适合通用 3D 内容

`Sampler::Descriptor` 默认值 SHALL 满足"开箱即用"的通用 3D 采样：

- `magFilter = LINEAR`
- `minFilter = LINEAR`
- `mipmapMode = LINEAR`
- `maxLod = 1000.0f`
- `minLod = 0.0f`
- 其它 wrap mode 默认 REPEAT，anisotropy 关闭

调用方 MAY 显式覆盖任意字段；本 requirement 仅约束默认值。

#### Scenario: 默认 sampler 采到 mipmap
- **WHEN** 创建带 mipmap 的纹理，使用 `Sampler::Descriptor{}` 默认值创建 sampler，远距离采样
- **THEN** 采到合适的 mip level（`maxLod=1000` 不再屏蔽 mipmap）

### Requirement: DeviceFeature 默认值统一为 false

`DeviceFeature` 结构体所有字段 SHALL 默认 `false`（包括 `meshShader`），表示"默认不假设支持"。后端在 `UpdateDeviceCaps` 时按实际能力打开。

#### Scenario: 默认构造的 DeviceFeature 全 false
- **WHEN** 创建 `DeviceFeature feat{};`
- **THEN** `feat.meshShader == false`；`feat.sparseBinding == false`；... 所有字段 false

### Requirement: Device::CreateResourceGroup 是 ResourceGroup 创建的正确入口

`Device` SHALL 提供 `CreateResourceGroup(const ResourceGroup::Descriptor &)` 接口；不再保留 `CreateSampler(const ResourceGroup::Descriptor &)` 这种基于 descriptor 类型的命名重载。

`Device` SHALL 无 `CreatePipelineLayout` 接口（native pipeline layout 由后端在 `Shader` 内派生，见 `aurora-shader-derived-layout`）。

#### Scenario: 调用方按正确名字创建

- **WHEN** 调用 `device->CreateResourceGroup({...})`
- **THEN** 接口存在；返回值符合 ResourceGroup 实际实现状态（本 change 不实现内容；`aurora-resource-group` 中实现）

#### Scenario: 旧名 CreateSampler(ResourceGroup::Descriptor) 不存在

- **WHEN** 调用方尝试 `device->CreateSampler(ResourceGroup::Descriptor{})`
- **THEN** 编译错误（接口已删除该 override）

#### Scenario: CreatePipelineLayout 不存在

- **WHEN** 调用方尝试 `device->CreatePipelineLayout({...})`
- **THEN** 编译错误（接口已删除，布局由 shader 反射派生）

### Requirement: Vulkan dynamic rendering 处理 stencil attachment

Vulkan `BeginRendering` 实现在 `DepthStencilAttachment::image` 的 PixelFormat 包含 stencil aspect（`hasStencil == true`，如 D24_S8 / D32_S8）时 SHALL 在 `VkRenderingInfo::pStencilAttachment` 上挂等价 attachment，载入 `stencilLoadOp` / `stencilStoreOp`。

format 不含 stencil 时 SHALL 不挂 `pStencilAttachment`（保持当前行为）。

#### Scenario: D32_S8 attachment + 显式 stencil clear
- **WHEN** RenderingInfo.depthStencil.image 是 D32_S8 image，stencilLoadOp=CLEAR, stencilStoreOp=STORE, clearValue.depthStencil={1.0, 0xFF}
- **THEN** Vulkan validation layer 不报"missing stencil attachment"；shader 中读 stencil 得到 0xFF（在 LoadOp 之后）

#### Scenario: D32 attachment 不挂 stencil
- **WHEN** RenderingInfo.depthStencil.image 是 D32（无 stencil）
- **THEN** `pStencilAttachment` 为 nullptr；不报错

### Requirement: Aurora 接口 namespace 全部统一为 sky::aurora

接口层所有头文件 SHALL 使用 `namespace sky::aurora`。

#### Scenario: VertexDecl.h 使用 sky::aurora
- **WHEN** include `<aurora/rhi/VertexDecl.h>`
- **THEN** `sky::aurora::VertexDesc` 可访问；`aurora::rhi::VertexDesc` 不存在（旧 namespace 已移除）

### Requirement: Encoder 顶点 / 视口 / 剪裁绑定数量上限通过常量声明

`aurora/rhi/Core.h` SHALL 暴露：
- `MAX_VERTEX_BINDINGS`（默认 16）
- `MAX_VIEWPORTS`（默认 16）
- `MAX_COLOR_ATTACHMENTS`（已存在，默认 8）

`Encoder::BindVertexBuffers` / `SetViewport` / `SetScissor` 在 count 超过对应常量时 SHALL 在 debug build 触发 assert（不再 silent clamp）。

#### Scenario: 常量可被外部代码引用
- **WHEN** include `<aurora/rhi/Core.h>` 后 `static_cast<uint32_t>(MAX_VERTEX_BINDINGS)`
- **THEN** 编译通过，值为 16

#### Scenario: 超出 MAX_VERTEX_BINDINGS 触发 debug assert
- **WHEN** debug build 调用 `BindVertexBuffers(0, 32, views)`
- **THEN** 触发 assert；release build 行为未定义但 MUST 不静默截断为 16

### Requirement: Semaphore 区分 binary 与 timeline 类型

`Semaphore::Descriptor` SHALL 包含 `SemaphoreType type`（默认 `BINARY`）与 `uint64_t initialValue`（仅 `TIMELINE` 使用，默认 0）。

`Semaphore` SHALL 提供 `GetType() -> SemaphoreType` 查询自身类型。

后端 MUST 按 type 选择底层实现：
- BINARY：Vulkan binary semaphore / DX12 ID3D12Fence（auto-incrementing）/ Metal MTLEvent / GLES 软实现
- TIMELINE：Vulkan timelineSemaphore / DX12 ID3D12Fence（caller-supplied value）/ Metal MTLSharedEvent / GLES 软实现（atomic + condvar）

#### Scenario: 创建 binary semaphore
- **WHEN** 调用 `device->CreateSema({.type = BINARY})`
- **THEN** 返回非空 `Semaphore*`，其 `GetType() == BINARY`

#### Scenario: 创建 timeline semaphore 带初始值
- **WHEN** 调用 `device->CreateSema({.type = TIMELINE, .initialValue = 5})`
- **THEN** 返回的 timeline semaphore 当前 host-side 可见 value 为 5

### Requirement: Timeline semaphore 支持 host-side signal 与 wait

对于 `TIMELINE` 类型的 `Semaphore`，SHALL 提供：
- `Signal(uint64_t value)`：host 端将 semaphore 推进到 `value`（要求 `value` 严格大于当前值）
- `Wait(uint64_t value, uint64_t timeoutNs) -> bool`：阻塞直到 semaphore 达到 ≥ `value`，或超时；超时返回 false
- `GetCurrentValue() -> uint64_t`：返回 host 当前可见的 value（轮询用）

对 `BINARY` semaphore 调用上述方法的行为是未定义（实现 MAY 通过 assert 触发错误）。

#### Scenario: host 端 signal 推进 timeline value
- **WHEN** 创建 initialValue=0 的 timeline semaphore，调用 `sema->Signal(10)`
- **THEN** `sema->GetCurrentValue() ≥ 10`；其它线程 `Wait(10, UINT64_MAX)` 立即返回 true

#### Scenario: timeline value 必须单调递增
- **WHEN** 调用 `Signal(5)` 后再 `Signal(3)`
- **THEN** 后端 MUST 拒绝（assert / 返回错误）；`GetCurrentValue()` 仍 ≥ 5

#### Scenario: Wait 超时返回 false
- **WHEN** 调用 `Wait(100, 1_000_000)`（1ms）但 semaphore 当前值仍是 0 且无人 signal
- **THEN** 阻塞约 1ms 后返回 false

### Requirement: SubmitInfo 中 SemaphoreSubmitInfo 携带 stage mask 与 value

`SemaphoreSubmitInfo` SHALL 包含：
- `Semaphore *semaphore`
- `uint64_t value`（仅 timeline 使用，binary 时被实现忽略）
- `PipelineStageFlags stageMask`（默认 `PipelineStageBit::TOP`，表示在该 stage 等待 / signal）

后端在转换为 native API 时 MUST 用 `stageMask` 对应到 `VkPipelineStage2` / `D3D12_BARRIER_SYNC` / `MTLRenderStages`。

#### Scenario: 跨队列 timeline 链
- **WHEN** queue A Submit signal `(timelineSema, value=10)`；queue B Submit wait `(timelineSema, value=10, stage=VERTEX_SHADER)`
- **THEN** queue B 的 cmdbuf 仅在 timelineSema 达到 10 后才在 vertex stage 开始执行；两次 Submit 之间无显式 host wait

### Requirement: Fence 提供非阻塞查询与重置

`Fence` SHALL 提供：
- `Wait()`（已有）：阻塞至 signaled
- `Reset()`（已有）：将 fence 置为 unsignaled
- `IsSignaled() -> bool`（**新增**）：非阻塞查询；signaled 返回 true，否则 false
- `WaitFor(uint64_t timeoutNs) -> bool`（**新增**）：带超时的 wait；超时返回 false

`Fence` MUST 仅与 `Queue::Submit` 配合使用（不能 host signal）。Submit 完成时 fence 被 signal；调用方 Reset 后可以复用。

#### Scenario: IsSignaled 在 Submit 完成前为 false
- **WHEN** Submit 一个含 cmdbuf 的 SubmitInfo 并立即 `fence->IsSignaled()`
- **THEN** 在 GPU 完成前返回 false；`fence->Wait()` 后 `IsSignaled()` 返回 true

#### Scenario: WaitFor 超时
- **WHEN** Submit 一个长任务并立即 `fence->WaitFor(0)`
- **THEN** 返回 false（任务尚未完成）；之后 `fence->Wait()` 阻塞至完成

#### Scenario: Reset 后复用
- **WHEN** Submit + `Wait()` 后调用 `fence->Reset()`，再次 Submit 同一 fence
- **THEN** `IsSignaled()` 在第二次 Submit 完成前为 false；`Wait()` 正常工作

### Requirement: Acquire 与 Present 必须用 binary semaphore

`SwapChain::AcquireNextImage` 的 `signalSema` 与 `SwapChain::Present` 的 `waitSemas` MUST 全部为 BINARY 类型。

后端在收到 timeline semaphore 时 MUST 拒绝（assert / 报错），因为大多数 native swapchain API 不接受 timeline semaphore。

#### Scenario: Present 拒绝 timeline semaphore
- **WHEN** 调用方误传 timeline semaphore 给 `swapchain->Present`
- **THEN** Debug build 下触发 assert；Release build 下行为未定义但 MUST 不静默成功

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
- Vulkan / DX12：直接落到 cmdbuf-级 native API
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

### Requirement: Queue 抽象按 QueueType 暴露

Aurora `Device` SHALL 通过 `Device::GetQueue(QueueType type)` 返回对应类型的 `Queue` 指针。`Queue` 实例的所有权归 `Device` 所有，调用方不得 delete；`Queue` 在 `Device` 析构后失效。

`QueueType` MUST 至少支持 `GRAPHICS`、`COMPUTE`、`TRANSFER` 三类。在硬件不支持独立 compute / transfer 队列的后端（GLES、或 Vulkan 上某些 GPU），`Device::GetQueue` MAY 对多个类型返回同一 `Queue*`，但行为 MUST 保持正确。

#### Scenario: 取得 graphics queue
- **WHEN** 调用方在已初始化的 `Device` 上调用 `device->GetQueue(QueueType::GRAPHICS)`
- **THEN** 返回非空 `Queue*`，其 `GetType()` 返回 `QueueType::GRAPHICS`

#### Scenario: 多次取同一队列返回相同实例
- **WHEN** 调用方两次调用 `device->GetQueue(QueueType::GRAPHICS)`
- **THEN** 返回的 `Queue*` 是同一指针（按 device 缓存）

#### Scenario: GLES 后端 compute / transfer 共享 queue
- **WHEN** 在 GLES 后端调用 `device->GetQueue(QueueType::GRAPHICS)`、`device->GetQueue(QueueType::COMPUTE)`、`device->GetQueue(QueueType::TRANSFER)`
- **THEN** 三次返回值可以是同一 `Queue*`，并且每个返回值 `GetType()` 报告对应的 `QueueType`（即 type 是查询参数而非 queue 真实属性）

### Requirement: Queue::Submit 提交命令缓冲

`Queue` SHALL 提供 `Submit(const SubmitInfo &info)` 方法，将 `info.commandBuffers` 中的所有命令按顺序提交到底层硬件队列执行。

`SubmitInfo` MUST 至少包含：
- `std::vector<CommandBuffer*> commandBuffers`（可为空，仅信号 / 等待）
- `std::vector<SemaphoreSubmitInfo> waitSemaphores`
- `std::vector<SemaphoreSubmitInfo> signalSemaphores`
- `Fence *fence`（可为 nullptr）

执行顺序：先在 `waitSemaphores` 全部满足后开始执行 `commandBuffers`，结束后按顺序 signal `signalSemaphores` 与（如有）`fence`。

调用方 MUST 在 `Submit` 返回后保持 `commandBuffers` 中的对象存活直到对应 fence 已 signaled 或对应 signalSemaphore 的 value 已被等待。

#### Scenario: 单 cmdbuf 提交无信号无 fence
- **WHEN** 调用方录制完一个 `CommandBuffer` 并构造仅含该 cmdbuf 的 `SubmitInfo`，调用 `queue->Submit(info)`
- **THEN** Submit 返回；之后调用 `queue->WaitIdle()` 后该 cmdbuf 的命令已在 GPU 上执行完毕

#### Scenario: Submit 携带 fence 完成查询
- **WHEN** 调用方传入未 signaled 的 `Fence*` 调用 Submit，并在 Submit 后立即调用 `fence->IsSignaled()`
- **THEN** 返回 `false`；调用 `fence->Wait()` 阻塞直至 GPU 完成；之后 `fence->IsSignaled()` 返回 `true`

#### Scenario: Submit signal binary semaphore 给后续 submit wait
- **WHEN** 调用方在第一次 Submit 中 signal binary semaphore S，在第二次 Submit 中 wait S
- **THEN** 第二次 Submit 的 cmdbuf 在第一次完成后才开始执行；可由 fence 或最终输出验证

#### Scenario: Submit 支持空 cmdbuf 列表用于纯信号传递
- **WHEN** 调用方构造 `commandBuffers` 为空但 `signalSemaphores` 非空的 SubmitInfo 调用 Submit
- **THEN** Submit 不报错，并在等待项满足后 signal 所有 signalSemaphores；fence（如有）也被 signal

### Requirement: Queue::WaitIdle 同步等待该队列空闲

`Queue` SHALL 提供 `WaitIdle()` 方法，阻塞调用线程直到该队列上**所有已提交命令**完成执行。

#### Scenario: WaitIdle 在 Submit 之后保证完成
- **WHEN** 调用方 Submit 一个 cmdbuf，紧接着调用 `queue->WaitIdle()`
- **THEN** WaitIdle 返回后，该 cmdbuf 中所有 GPU 命令都已完成（可由后续 buffer readback 验证）

### Requirement: Device::WaitIdle 等待全部队列

`Device::WaitIdle()` SHALL 在返回前等待该 device 上**所有 Queue** 的全部已提交命令完成（等价于对 graphics / compute / transfer 各自调用 WaitIdle，且不规定先后）。

#### Scenario: 多队列均空闲
- **WHEN** 多次向不同队列 Submit 后调用 `device->WaitIdle()`
- **THEN** 返回时所有队列上的命令均已完成

### Requirement: SwapChain 状态自查

`SwapChain` SHALL 提供 `SwapChainStatus GetStatus() const`，枚举 `{ OK, OUT_OF_DATE, LOST }`。`GetStatus()` SHALL 由后端自查：surface 尺寸由原生窗口决定，后端查询 `Descriptor.window` 当前尺寸与自身 extent 比对。

- 一致 → `OK`；
- 不一致 → `OUT_OF_DATE`；
- surface 丢失 → `LOST`。

`SUBOPTIMAL` SHALL 当作 `OK` 处理。RHI 层 SHALL NOT 决定目标尺寸（尺寸来自原生窗口）。

#### Scenario: 尺寸一致返回 OK

- **WHEN** swapchain extent 与原生窗口当前尺寸一致
- **THEN** `GetStatus()` 返回 `OK`

#### Scenario: 尺寸不一致返回 OUT_OF_DATE

- **WHEN** 原生窗口被 resize，其尺寸与 swapchain extent 不一致
- **THEN** `GetStatus()` 返回 `OUT_OF_DATE`（调用方据此触发 `Resize`）

#### Scenario: surface 丢失返回 LOST

- **WHEN** surface 丢失（如 Vulkan `VK_ERROR_SURFACE_LOST_KHR`）
- **THEN** `GetStatus()` 返回 `LOST`

### Requirement: SwapChain 创建与基础属性查询

`Device::CreateSwapChain(const SwapChain::Descriptor &)` SHALL 返回一个完整的 `SwapChain` 实例。`Descriptor` MUST 包含 `void* window`、`uint32_t width`、`uint32_t height`、`PixelFormat preferredFormat`、`PresentMode preferredMode`。

后端 MAY 协商出与 `preferredFormat` / `preferredMode` 不同的实际 format / mode；调用方 MUST 通过 `GetFormat()` 取得最终使用的 format。

`SwapChain` SHALL 至少提供以下查询：
- `GetImageCount() -> uint32_t`：swapchain 中可呈现 image 的数量
- `GetFormat() -> PixelFormat`：实际使用的 color format
- `GetExtent() -> Extent2D`：当前 image 尺寸
- `GetImage(uint32_t index) -> Image*`：取得索引对应的 backbuffer

`GetImageCount()` MUST 返回 ≥ 2。`GetImage(i)` 对所有 `0 ≤ i < GetImageCount()` MUST 返回非空 `Image*`，该 `Image` 持有 `ImageUsageFlagBit::RENDER_TARGET`。

#### Scenario: 创建并查询 swapchain
- **WHEN** 调用方在持有 native window 的环境下创建 `SwapChain(800, 600, BGRA8_UNORM, IMMEDIATE)`
- **THEN** 返回非空 `SwapChain*`；`GetImageCount() ≥ 2`；`GetExtent() == {800, 600}`；`GetFormat()` 是后端实际选用的 format

#### Scenario: SwapChain image 可作为 ColorAttachment
- **WHEN** 调用方对 `swapchain->GetImage(0)` 构造 `RenderingInfo::colors[0].image` 并 BeginRendering
- **THEN** Encoder 接受该 image，能正常 BeginRendering / EndRendering

### Requirement: AcquireNextImage 取得待渲染索引

`SwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) -> uint32_t` SHALL 返回下一个可被渲染的 image 索引。

- `signalSema`（如非空）MUST 为 binary semaphore；当 image 真正可用时被 signal
- `fence`（如非空）：image 可用时 fence 被 signal
- `timeoutNs == UINT64_MAX` 表示无限等待
- 返回 `INVALID_INDEX` 表示超时或 swapchain 失效

调用方 MUST 在使用 `GetImage(index)` 录制命令前先 wait `signalSema`（在 Queue::Submit 的 waitSemaphores 中传入）或 wait `fence`。

#### Scenario: Acquire 返回有效索引
- **WHEN** swapchain 刚创建并调用 `AcquireNextImage(sema, nullptr, UINT64_MAX)`
- **THEN** 返回的索引在 `[0, GetImageCount())` 范围；后续提交一个 wait `sema` 的 cmdbuf 不会死锁

#### Scenario: Acquire 不传 sema 与 fence 退化为同步
- **WHEN** 调用 `AcquireNextImage(nullptr, nullptr, UINT64_MAX)`
- **THEN** 返回有效索引；后端 MUST 在返回前确保 image 已可用（实现可用 hidden internal sync）

### Requirement: Present 呈现图像到屏幕

`SwapChain::Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas)` SHALL 将 `imageIndex` 对应的 image 呈现到关联的 native window。

Present MUST 等待 `waitSemas` 中所有 binary semaphore signal 后才将 image 提交给 windowing system。

调用方 MUST 在 Present 之前 Submit 含该 image 渲染命令的 cmdbuf，并把 cmdbuf signal 的 binary semaphore 作为 `waitSemas` 之一传入。

#### Scenario: 完整一帧 clear-screen
- **WHEN** 依次调用：Acquire(signalSemaA)；录制 cmdbuf 把 image clear 为已知颜色；Submit（waitSemaA + signalSemaB）；Present(imageIndex, 1, &semaB)
- **THEN** 整个流程不报错；`device->WaitIdle()` 之后下一帧 Acquire 又返回有效索引（说明 swapchain 仍处于工作状态）

### Requirement: Resize 在窗口尺寸变化时重建

`SwapChain::Resize(uint32_t width, uint32_t height)` SHALL 释放当前 swapchain 的全部 image 并按新尺寸重新创建。Resize 之前持有的 `Image*`（来自 `GetImage`）在 Resize 后**全部失效**，调用方 MUST 在 Resize 之后重新调用 `GetImage` 取得新指针。

Resize 时如有未完成的 Submit 涉及旧 image，调用方 MUST 先 `device->WaitIdle()`（或等价的 fence 等待），否则行为未定义。

#### Scenario: Resize 后 GetExtent 反映新尺寸
- **WHEN** 在 swapchain（800×600）上调用 `device->WaitIdle()` 后 `Resize(1024, 768)`
- **THEN** `GetExtent() == {1024, 768}`；`GetImage(0)` 返回新指针（与 Resize 前的指针不要求相等）；`GetImageCount()` MAY 改变也 MAY 不变

#### Scenario: Resize 至零尺寸
- **WHEN** 调用 `Resize(0, 0)`（窗口最小化）
- **THEN** Resize 不崩溃；后续 `AcquireNextImage` MAY 返回 `INVALID_INDEX`；调用方应在窗口恢复时再次 Resize 到非零尺寸

### Requirement: SwapChain 销毁

`SwapChain` 销毁时 SHALL 释放所有 backbuffer image、native swapchain handle、与之关联的 surface。销毁前调用方 SHOULD 确保该 swapchain 的所有 in-flight Present 已完成（一般通过 `device->WaitIdle()`）。

#### Scenario: 正常销毁
- **WHEN** 调用 `device->WaitIdle()` 后释放最后一个 `CounterPtr<SwapChain>`
- **THEN** 析构正常完成，不泄漏 native handle（valgrind / VkValidationLayer / D3D12 debug layer 不报错）

### Requirement: 各后端 Device::CreateFrameContext 返回真实实现

`Device::CreateFrameContext(const DeviceFrameContextInitInfo&)` SHALL 在所有已落地后端返回非空的 `DeviceFrameContext*`：

- **Vulkan**：`VulkanDevice::CreateFrameContext` 返回 `new VulkanDeviceFrameContext(this, info)`（不再返回 nullptr）
- **DX12**：`D3D12Device` 声明并实现 `CreateFrameContext` override，返回 `new D3D12DeviceFrameContext(this, info)`
- **Metal**：保持现有 `MetalDeviceFrameContext` 行为

#### Scenario: Vulkan 返回非空 frame context
- **WHEN** 在 Vulkan 后端调用 `device->CreateFrameContext({})`
- **THEN** 返回非空指针

#### Scenario: DX12 返回非空 frame context
- **WHEN** 在 DX12 后端调用 `device->CreateFrameContext({})`
- **THEN** 返回非空指针，且 `D3D12Device` 不再是抽象类（可实例化）

### Requirement: FrameContext 按 inflightNum 预分配 command buffer

每个后端的 `DeviceFrameContext` 构造时 SHALL 通过 `Device::CreateCommandPool(QueueType::GRAPHICS)` 创建 graphics command pool，并调用 `CommandPool::Allocate()` 预分配恰好 `inflightNum` 个 command buffer。

#### Scenario: inflightNum 控制 buffer 数量
- **WHEN** 调用 `CreateFrameContext({inflightNum = 3})`
- **THEN** frame context 持有 3 个 command buffer（`mBuffers.size() == 3`）

#### Scenario: 默认 inflightNum 为 2
- **WHEN** 调用 `CreateFrameContext({})`
- **THEN** frame context 持有 2 个 command buffer

### Requirement: FrameContext 按 parallelNum 预分配并行编码 command buffer

每个后端的 `DeviceFrameContext` 构造时 SHALL 根据 `DeviceFrameContextInitInfo.parallelNum`（默认 1）决定是否建立并行编码：当 `parallelNum > 1` 时，构造 worker 线程池并按 worker 预分配 `parallelNum × inflightNum` 个 parallel command buffer；当 `parallelNum == 1` 时不额外分配。

并行缓冲的分配来源 SHALL 按后端：
- **Vulkan**：每个 worker 的 `VulkanContext` 持有 `VK_COMMAND_BUFFER_LEVEL_SECONDARY` command pool，从中分配 secondary command buffer。
- **DX12**：每个 worker 的 `D3D12Context` 持有独立的 DIRECT command pool（独立 allocator），从中分配 command list。
- **Metal**：FrameContext 持有额外的 `MetalCommandPool` 分配 command buffer；worker 的 `MetalThreadContext` 仅提供 autorelease pool。

#### Scenario: parallelNum > 1 时分配 parallel buffers
- **WHEN** 调用 `CreateFrameContext({inflightNum = 3, parallelNum = 2})`
- **THEN** frame context 持有 3 个 primary buffer 与 6 个 parallel buffer（`parallelNum × inflightNum`）

#### Scenario: 默认 parallelNum 为 1 不额外分配
- **WHEN** 调用 `CreateFrameContext({})`（`parallelNum` 默认 1）
- **THEN** frame context 仅持有 primary buffer，不构造 worker 线程池 / parallel buffer

### Requirement: 后端 frame context 目录与命名对齐 Metal rdg 约定

Vulkan / DX12 的 frame context 头文件与源文件 SHALL 置于各自后端的 `include/rdg/` 与 `src/rdg/` 目录，类名分别为 `VulkanDeviceFrameContext` / `D3D12DeviceFrameContext`，继承自 `DeviceFrameContext`，并通过后端 CMake 的 `GLOB_RECURSE` 自动纳入构建。

#### Scenario: 头文件可被后端 device 引用
- **WHEN** 编译 Vulkan 后端
- **THEN** `#include <rdg/VulkanDeviceFrameContext.h>` 可解析（无需改动 CMake），`VulkanDeviceFrameContext` 可见

#### Scenario: DX12 编译通过
- **WHEN** 在 Windows 上编译 DX12 后端
- **THEN** `D3D12DeviceFrameContext` 编译链接成功，`CreateFrameContext` override 满足纯虚

### Requirement: ThreadContext 由 FrameContext 初始化并持有生命周期

并行编码的 worker `ThreadContext` SHALL 在 `DeviceFrameContext` 内部初始化，并由 `DeviceFrameContext` 持有其生命周期（通过其持有的 ThreadPool）。`Device` SHALL 不再提供 `CreateAsyncContext` 主动创建接口，也不再暴露 `GetParallelContext()`；上层通过 `DeviceFrameContext::GetParallelContext()` 获取并行线程池。

#### Scenario: Device 不再提供 CreateAsyncContext
- **WHEN** 调用 `device->CreateAsyncContext(QueueType::GRAPHICS)`
- **THEN** 编译错误（接口已移除）

#### Scenario: FrameContext 暴露并行线程池
- **WHEN** `parallelNum > 1` 时调用 `frameContext->GetParallelContext()`
- **THEN** 返回非空 ThreadPool，worker 数 = `parallelNum`

### Requirement: 单线程构建 + 批次提交

`DeviceFrameDispatcher` SHALL 采用单线程构建、批次提交模型：`CreateTask` / `DependsOn` 仅在调用线程（构建期）执行，`Submit` 提交整批，批次内所有节点执行完毕后统一释放节点内存。

调用方 MUST 在 `Submit` 前完成全部 `CreateTask` / `DependsOn`；`Submit` 之后不得再增删节点。节点内存由批次整体持有，不按单节点引用计数。

#### Scenario: 构建后整体提交

- **WHEN** 调用方在构建期创建 N 个节点并建立依赖，然后 `Submit(pool)`
- **THEN** 整批节点被调度执行，批次 future 在所有节点完成后 ready

#### Scenario: 批次整体释放

- **WHEN** 批次所有节点执行完成
- **THEN** 节点内存随 `Clear` 或析构统一释放，节点间无逐节点引用计数

### Requirement: 节点 index 引用与连续存储

节点 SHALL 以 `NodeIndex`（`uint32_t`）标识，`children` SHALL 存子节点 index 而非指针。节点对象 SHALL 连续存储于批次持有的容器中，`Submit` 后该容器只读、不再重分配。

#### Scenario: children 用 index 引用

- **WHEN** 一个节点声明依赖多个父节点
- **THEN** 每个父节点的 `children` 存储子节点的 index，每边 4 字节

### Requirement: 无锁无引用计数调度

`DependsOn` SHALL 不持锁（单线程构建）；`Submit` 后子节点遍历为只读，无需锁。节点无引用计数成员，不执行 `AddRef` / `RemoveRef`。

#### Scenario: 构建与执行零锁零计数

- **WHEN** 构建期建立依赖、执行期父节点完成并调度子节点
- **THEN** 不涉及 `SpinLock`、`std::mutex`，也不发生引用计数增减

### Requirement: per-node 与批次级 future

`GetFuture(node)` SHALL 返回单节点完成 future，且仅在构建期调用（`Submit` 前）。`Submit(pool)` SHALL 返回批次级 future，信号所有节点完成。

#### Scenario: per-node future 构建期获取

- **WHEN** 构建期对某节点调用 `GetFuture(node)` 后在 `Submit` 后 `wait`
- **THEN** 该节点执行完成后 future ready

#### Scenario: 批次级 future 覆盖整体完成

- **WHEN** `Submit(pool)` 返回的 future 被 `wait`
- **THEN** 该 future 在所有节点执行完成后 ready

### Requirement: 并行执行依赖图

`DeviceFrameDispatcher` SHALL 通过 `ThreadPool::Schedule` 将可执行节点提交到 worker 队列（round-robin + work-stealing），多 worker 并行执行无依赖的节点；父节点完成后递减子节点父计数，归零时子节点入队。

#### Scenario: 依赖顺序正确

- **WHEN** 一个线性链 / diamond / 多根依赖图被 `Submit` 并 `wait`
- **THEN** 每个节点仅在其所有父节点完成后执行一次，无丢失、无重复、无死锁

### Requirement: DescriptorBatch 接口

`DescriptorBatch` SHALL 是接口层抽象，跨多个 `ResourceGroup` 累积写入，`Flush()` 一帧提交一次，`Reset()` 帧末复用：

- `WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0)`
- `WriteImage(ResourceGroup *group, uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0)`
- `WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0)`
- `Flush()` / `Reset()`

`Device::CreateDescriptorBatch()` SHALL 创建 batch。`DeviceFrameContext` SHALL 按 `inflightNum` 持有 batch，`BeginFrame` 切当前并 `Reset()`。

#### Scenario: 跨 set 累积单次提交

- **WHEN** 对 set 0 / set 1 的两个 group 各自 `WriteBuffer` 后 `Flush()`
- **THEN** Vulkan 用一次 `vkUpdateDescriptorSets` 写入两个 set；后续 Bind + Draw 各自采样正确

### Requirement: 延迟释放（deferred release）

被替换的 descriptor set（及其 buffer/image 引用）SHALL **不立即释放**，而是登记到当前 in-flight frame 的 retire 队列，等该帧 fence 完成（`inflightNum` 帧后）再归还 pool / 释放。

#### Scenario: 旧 set 延迟释放

- **WHEN** 帧 N 更新某 RG（换新 set），旧 set 登记到帧 N 的 retire 队列
- **THEN** 旧 set 在帧 N+inflightNum 退役前保持存活；退役后归还 pool

### Requirement: Global/Pass 持久化 + 罕见更新换新

Global（set 0）/ Pass（set 1）SHALL **持久化**：默认持一个 set 写一次长期复用（低更新频率）。需要更新时 SHALL 重新申请一个新 set，绑定新 set，旧 set 走 retire 队列延迟释放。

#### Scenario: 默认不重分配

- **WHEN** 连续多帧 Global set 内容不变
- **THEN** 不重新分配 set，复用同一 set（无 per-frame 分配）

#### Scenario: 偶发更新换新 + 延迟释放

- **WHEN** 场景/视图变化触发 Global 更新
- **THEN** 申请新 set 并绑定；旧 set 登记 retire 队列，`inflightNum` 帧后归还

### Requirement: Batch 尽可能持久化 + cached 内容

Batch（set 2）SHALL **尽可能持久化**：descriptor set 对象复用，不每帧重分配。内容变化时（每帧 pack 可能拿到不同 buffer、纹理 streaming upgrade/downgrade）SHALL 更新对应 binding，被替换的 buffer/image 引用走 retire 队列延迟释放。

为降低每帧 buffer 变化带来的 descriptor 重写，SHALL 优先保持 pack buffer handle 稳定（单一持久 buffer 内部分段，dynamic offset 选数据）；只有 buffer 真正换掉（扩容/换池）时才更新 descriptor binding。

#### Scenario: pack buffer handle 稳定

- **WHEN** pack buffer 是单一持久 buffer，帧间只改 dynamic offset
- **THEN** descriptor binding 不更新（handle 稳定），无重写

#### Scenario: buffer 换池时更新 binding

- **WHEN** pack buffer 扩容/换池导致 buffer handle 变化
- **THEN** 更新 descriptor binding 指向新 buffer；旧 buffer 引用延迟释放

#### Scenario: streaming upgrade/downgrade

- **WHEN** 材质纹理 streaming 换 mip 级别（换 image view）
- **THEN** 仅更新 image binding 指向新 view；旧 view 引用延迟释放；其余 binding cached 不动

### Requirement: cache key 下沉 Vulkan（bufferid/viewid/samplerid）

cached 内容的 cache key 整套下沉 Vulkan 内部：`VulkanBuffer` / `VulkanImage` / `VulkanSampler` 包装对象持单调 ID（`VulkanDevice` 创建时发号，永不复用），view 由 `GetView(ViewDesc)` 发单调 viewid。cached-content key = `(binding, bufferId / viewid / samplerId)`，MUST 不用原生 handle（会被驱动回收复用）或对象指针（地址可能复用）。接口层不暴露 resource ID、不感知 cache key。

#### Scenario: 对象复用不误判

- **WHEN** 旧 buffer 销毁后新 buffer 创建（Vulkan 侧 ID 单调递增，二者 ID 必然不同）
- **THEN** cached 内容比较 ID 判为「已变」，触发 descriptor 重写，无误判

### Requirement: in-flight 资源隔离

`DescriptorBatch` 累加缓冲、Vulkan descriptor pool（含 retire 队列）、DX12 shader-visible ring、Batch pack buffer SHALL 都按 `inflightNum` 分帧轮换，帧退役（fence 完成）后复用。

#### Scenario: 帧间资源不冲突

- **WHEN** `inflightNum=2`，帧 0 与帧 1 各自持有独立的 batch/pool/ring 段/pack buffer
- **THEN** 帧 1 的写入不覆盖帧 0 仍在 GPU 使用的资源

### Requirement: PipelineState 携带顶点输入布局

`PipelineState` SHALL 携带 `std::vector<VertexBindingDesc> vertexBindings` 与 `std::vector<VertexAttributeDesc> vertexAttributes`。`VertexAttributeDesc` SHALL 含 `semantic`（名称）与 `semanticIndex`（序号，供 DX12 `SemanticName` + `SemanticIndex` 使用）。后端 PSO 创建 SHALL 能从该布局派生原生输入布局（输入装配路径）。

#### Scenario: 布局随 pipeline state 传递

- **WHEN** 调用方在 `PipelineState` 中声明 binding（stride/inputRate）与 attribute（location/binding/offset/format/semantic/semanticIndex）
- **THEN** `GraphicsPipeline::Descriptor::state` 中可读到相同布局

#### Scenario: 未声明布局时为空

- **WHEN** 不设置 `vertexBindings` / `vertexAttributes`
- **THEN** 两容器为空，后端按空输入布局创建（兼容 fullscreen / 顶点拉取路径）

### Requirement: Buffer 暴露分配大小

`Buffer` SHALL 提供 `GetSize()`（默认返回 0，后端可覆写）。需要按 buffer 推导视图大小的后端（如 DX12 index buffer view）SHALL 使用该访问器，SHALL NOT 依赖外部传入的 `range` 或硬编码 0。

#### Scenario: 后端返回真实大小

- **WHEN** 查询一个已创建 buffer 的 `GetSize()`
- **THEN** 返回其分配的字节数

#### Scenario: 默认实现

- **WHEN** 某后端未覆写 `GetSize()`
- **THEN** 返回 0，调用方据此跳过大小推导

### Requirement: Device 暴露当前 RHI API

Aurora `Device` SHALL expose the active RHI `API` (through `Device::GetAPI()`), implemented by every backend, so
consumers can adapt behavior (for example the shader compilation target) to the backend actually in use.

#### Scenario: Query the active API
- **WHEN** a consumer holds a `Device*` created through `Instance::Init`
- **THEN** `Device::GetAPI()` SHALL return the backend in use (`VULKAN` / `DX12` / `METAL`)

#### Scenario: Backends implement the accessor
- **WHEN** any of the Vulkan, DX12 or Metal backends is built
- **THEN** its device SHALL implement `GetAPI()` and SHALL NOT fall back to a default value

### Requirement: Device 暴露 clip-space Y 方向

Aurora `Device` SHALL expose the clip-space Y axis of the active backend (`DeviceCapability::clipSpaceYDown`),
filled by each backend in `UpdateDeviceCaps()`, so projection builders can produce the correct vertical orientation
on every backend.

#### Scenario: Vulkan is Y-down
- **WHEN** the active backend is Vulkan
- **THEN** `clipSpaceYDown` SHALL be `true`

#### Scenario: DX12 and Metal are Y-up
- **WHEN** the active backend is DX12 or Metal
- **THEN** `clipSpaceYDown` SHALL be `false`

