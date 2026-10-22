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

Queue SHALL 提供 Submit 协议，将 SubmitInfo 的 commandBuffers 提交至对应队列，并返回可观察的接受结果。结果 SHALL 区分 Accepted、已知未入队的 Rejected，以及 DeviceLost/无法证明是否入队的错误；具体类型命名由实现确定。Accepted SHALL 不表示 GPU 完成。SubmitInfo SHALL 至少支持 commandBuffers、带 stage/value 的 waitSemaphores/signalSemaphores 和可选 fence；空 commandBuffers 的同步提交 SHALL 合法。

后端 SHALL 按各同步原语的作用范围执行 wait/signal；调用方 SHALL 保活 command buffers 及相关资源直到已证明对应 GPU 使用结束。提交错误 SHALL 反馈给调用方，不能仅日志记录；不确定错误不得被调用方当成已知未提交或成功完成。

新增可观察入口 SHALL 在任何 native queue 操作前完成本地参数校验和暂存数组分配，并保留 native 错误码；同类型但属于其它 device 的对象 SHALL 被拒绝。后端尚未实现新协议时 SHALL 明确返回 Unsupported，不得通过调用旧 void Submit 伪造 Accepted。现有 legacy 入口 MAY 保留兼容，但 Renderer 新路径 SHALL 使用结果协议。

#### Scenario: 预检不得部分提交等待
- **WHEN** 输入 wait 合法但后面的 signal 或 command buffer 无效
- **THEN** 整次请求返回 Rejected，不能先把前缀 wait 排入队列而阻塞后续无关提交

#### Scenario: DX12 部分排队后失败
- **WHEN** native Wait/Execute/Signal 已开始，随后调用失败或检测到 device removed
- **THEN** 返回 Indeterminate 及错误，资源继续保活；调用方不能按 Rejected 立即回收

#### Scenario: 单 cmdbuf 提交无信号无 fence
- **WHEN** 调用方提交一个有效 command buffer 并取得 Accepted
- **THEN** 之后成功的 Queue::WaitIdle 可证明该队列工作完成；Submit 返回本身不能证明完成

#### Scenario: Submit 携带 fence 完成查询
- **WHEN** 有效 Submit 携带已 reset 的 fence 且被 Accepted
- **THEN** fence 在对应 GPU 工作完成时 signal；立即查询可能 Pending 或 Complete，不保证 GPU 一定尚未完成

#### Scenario: Submit signal binary semaphore 给后续 submit wait
- **WHEN** 第一次有效提交 signal binary semaphore S，第二次有效提交 wait S
- **THEN** 后续消费者在所声明同步范围满足之前不执行相应访问，且 binary semaphore 只按合法生命周期复用

#### Scenario: Submit 支持空 cmdbuf 列表用于纯信号传递
- **WHEN** commandBuffers 为空且有合法 wait/signal/fence 配置
- **THEN** 可接受该同步提交，满足等待后完成对应信号

#### Scenario: fence reset 后提交被拒绝
- **WHEN** 底层已知没有接受该提交
- **THEN** 返回 Rejected 及错误信息，上层不把 slot 留在等待该 fence 的正常 Submitted 状态

#### Scenario: 提交遇到 device lost 或不确定错误
- **WHEN** 后端不能证明提交未入队或设备已丢失
- **THEN** 反馈对应错误，上层隔离潜在在途资源并停止正常复用，走设备错误退休协议

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
- 当 surface 或 device 丢失时，后端 MUST 将 swapchain 状态置为 `LOST` 并返回 `INVALID_INDEX`，不得继续无限阻塞

调用方 MUST 在使用 `GetImage(index)` 录制命令前先 wait `signalSema`（在 Queue::Submit 的 waitSemaphores 中传入）或 wait `fence`。

#### Scenario: Acquire 返回有效索引

- **WHEN** swapchain 刚创建并调用 `AcquireNextImage(sema, nullptr, UINT64_MAX)`
- **THEN** 返回的索引在 `[0, GetImageCount())` 范围；后续提交一个 wait `sema` 的 cmdbuf 不会死锁

#### Scenario: Acquire 不传 sema 与 fence 退化为同步

- **WHEN** 调用 `AcquireNextImage(nullptr, nullptr, UINT64_MAX)`
- **THEN** 返回有效索引；后端 MUST 在返回前确保 image 已可用（实现可用 hidden internal sync）

#### Scenario: Surface lost 上报为 LOST

- **WHEN** 关联窗口被销毁导致 surface 丢失后调用 `AcquireNextImage`
- **THEN** 后端 SHALL 返回 `INVALID_INDEX` 且 swapchain 状态 SHALL 为 `LOST`（由调用方/事件层避免再次 acquire）

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

每个后端 DeviceFrameContext SHALL 持有由 inflightNum 决定的唯一 GPU slot ring，每个 slot 预留 coordinator 主编码资源；可复用后端 primary buffer/list 包装，但不得强迫所有 native API 使用 Vulkan primary/secondary 模型。RDG 路径 SHALL 在 context 内通过有效 DeviceFrameLease 使用资源，宿主不另建 command ring。command pool/allocator 的 reset 域 SHALL 与退休域一致，不得重置其它 slot 仍在途或同 slot 已录完待提交的命令资源。

#### Scenario: inflightNum 控制 buffer 数量
- **WHEN** 调用 CreateFrameContext 且 inflightNum 为 3
- **THEN** context 持有 3 份 coordinator 主编码资源，并在各自有效 lease 下使用，宿主无需另建 command ring

#### Scenario: 默认 inflightNum 为 2
- **WHEN** 使用默认初始化参数
- **THEN** 持有 2 份独立的 slot 主编码资源

#### Scenario: 一个 slot 完成而另一个仍在途
- **WHEN** 重用已完成 slot 的 command 资源
- **THEN** 不 reset 包含其它 pending command buffer 的共享 pool，也不 reset 仍被执行的 DX12 command allocator

### Requirement: FrameContext 按 parallelNum 预分配并行编码 command buffer

DeviceFrameContextInitInfo.parallelNum SHALL 只控制 GPU 编码 worker；默认 1 时不创建额外 worker pool，使用后端串行 dispatch；大于 1 时按 worker × inflightNum 预备独立 recording arena/context。Vulkan/DX12 MAY 预分配一份 buffer/list 作为每分区起始容量，任务数大于 worker 数时 SHALL 保留多个 recording ordinal 的结果或合法合并任务，不能 reset 上一个待提交结果。Metal SHALL 可使用 parent 分配的 child encoder，不强制固定数量 secondary buffers。reset 域 SHALL 隔离其它 worker/slot 的录制和 GPU 使用。

Vulkan SHALL 使用 worker 所属 command pool 的安全 slot 分区；DX12 SHALL 独占 DIRECT command list/allocator；Metal SHALL 保留 worker autorelease context 和显式持有的 command 资源。任务 SHALL 从实际执行 ThreadContext 取得资源，除非调度器明确提供亲和性，不得假定任务索引等于 worker 索引。worker 数 SHALL 不被解释为 CPU ECS prepare 的并行度。

#### Scenario: parallelNum > 1 时准备 recording 分区
- **WHEN** inflightNum 为 3 且 parallelNum 为 2
- **THEN** context 有 3 个主编码分区和 6 个 worker/slot recording 分区；native buffers/child encoders 根据后端计划及预算供给

#### Scenario: 默认 parallelNum 为 1 不额外分配
- **WHEN** 使用默认 parallelNum
- **THEN** 不创建编码 worker pool；仍走本后端计划的串行 dispatch，renderer 可另用专用 CPU pool 并行 prepare

#### Scenario: 单 worker 编码多个任务
- **WHEN** 同一 worker 在一个 slot 连续编码多个 native 单元
- **THEN** 各单元结果保持有效到提交/退休，不覆盖前一个结果，不并发使用同一 allocator 或普通 encoder

#### Scenario: 实际 worker 与编译任务编号不同
- **WHEN** 调度器把同一类编码任务交给不同实际 worker，且任务总数超过 worker 数
- **THEN** 每次从实际 ThreadContext 取得 slot 分区和独立 recording ordinal；任务编号不用于推断 worker，已录结果保持有效到退休

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

### Requirement: SwapChain 显式获取与呈现退休

SwapChain SHALL 为 checked 帧提供有界 acquire、可重试 present、未呈现释放、resize/close 和独立的 presentation 资源退休结果，保留具体原生错误。直接宿主协议与 checked 协议 SHALL 互斥。graphics completion SHALL 不单独充当 presentation 退休证明；超时不表示成功或安全释放。

RHI SHALL 提供不 acquire 的显式 surface 查询结果，保留查询错误并区分 surface 有效性、暂停、所选配置需要更新/重建与设备错误。后端 SHALL 根据最新能力与输出策略比较已应用配置，检查 extent、transform 及其它相关创建约束；不得仅检查尺寸或在查询失败时返回旧尺寸作为成功结果。renderer SHALL 不依赖 native caps 结构；更新与销毁仍受在途责任约束。
查询结果 SHALL 包含规范化的 extent/format/transform 和成功创建代次；可用结果表示已应用配置，需要重建的结果表示待应用配置，失败结果 SHALL 不作为准备快照。创建代次 SHALL 不随重复查询增加。

#### Scenario: Vulkan transform 变化但尺寸不变
- **WHEN** 策略选择 preTransform=currentTransform，查询得到新的 currentTransform 且 extent 与当前 swapchain 相同
- **THEN** 后端仍报告配置需要重建，校验所选 transform 的支持性，并向 renderer 提供一致的输出映射；能力枚举顺序变化或不影响所选配置的新增能力不单独触发重建

#### Scenario: surface 查询失败与未指定尺寸
- **WHEN** 原生 surface 查询失败，或 Vulkan currentExtent 为未指定值
- **THEN** 查询失败保留原生错误；未指定尺寸按平台 drawable 像素尺寸和能力范围解析，不能把未指定值当真实 extent，也不能用旧 swapchain 尺寸隐藏变化

#### Scenario: acquire 返回 Suboptimal
- **WHEN** 原生 acquire 成功取得图像但报告 Suboptimal
- **THEN** 保留该图像和同步的完整责任并安排或立即进行配置复核，不丢弃成功结果；相同有效配置不因重复 Suboptimal 每帧无条件重建，OutOfDate 与 SurfaceLost 继续走各自错误路径

#### Scenario: Vulkan 正常呈现与未提交释放
- **WHEN** 使用支持 swapchain-maintenance1 的 Vulkan 设备获取输出
- **THEN** 获取信号、消费提交、presentation fence 和 CPU 借用分别管理；正常复用等待全部责任完成。未提交图像在 acquire fence 确认后显式释放，不等待从未入队的 render-done signal；能力不足返回 Unsupported

#### Scenario: 原生呈现可重试与已排队错误
- **WHEN** Vulkan present 返回 OOM、OutOfDate 或 SurfaceLost，或 DXGI Present 返回 WAS_STILL_DRAWING
- **THEN** 已知未入队的失败保留同帧重试；可能已排队的呈现保留同步和图像责任直到安全退休，错误不被当作完整成功或无限 Pending

#### Scenario: DX12 呈现资源交还
- **WHEN** graphics completion 已确认并成功进行 DO_NOT_WAIT Present
- **THEN** 记录独立的 Present 后 queue fence 和对应 native buffer 所有权条件；它证明应用资源可退休，不宣称屏幕扫描完成。信号失败保留不确定责任，resize/close 不能提前释放仍在途 buffer

#### Scenario: 输出借用跨 lease 或绕过同步
- **WHEN** borrowed output 被编译到另一 DeviceFrameLease，或提交未携带其输出同步所有者
- **THEN** 显式拒绝且不排队；borrowed image 不转换为会删除 swapchain wrapper 的拥有型引用。Accepted/Indeterminate 保留同步 owner 至 GPU 安全收尾，Rejected 保留调用方录制

#### Scenario: 正常关闭与终态退出
- **WHEN** 输出请求 resize/close 或发生无法恢复的提交/退休错误
- **THEN** 正常路径只在该输出 CPU/GPU/presentation 责任清空后执行并返回确认，不等待整个设备；终态隔离不伪报正常确认，销毁保持 native 安全等待和 Device/module/window 寿命

### Requirement: 提交完成查询可区分错误

RHI SHALL 为 scheduler 提供非阻塞或有界的完成查询，区分 Pending、Complete 和 Error/DeviceLost。现有 bool 查询可保留兼容包装，但 renderer SHALL 使用可观察错误的协议，不将永久设备错误解释为无限 Pending。timeout SHALL 只表示尚未确认完成，不能作为资源安全回收证明。

#### Scenario: GPU 在途时设备丢失
- **WHEN** 已接受的 submission 在后续查询期间发生 device lost
- **THEN** 查询返回错误状态，scheduler 不继续无限等待普通 fence，也不把该提交资源当成正常完成后可复用

#### Scenario: fence reset 与提交分别可观察
- **WHEN** idle fence reset 成功后提交被明确拒绝
- **THEN** fence 仍可为 Pending，但不存在可等待的提交；重试或丢弃由 slot 状态决定，不能把 Pending 当作入队证据

#### Scenario: DX12 device removed 哨兵
- **WHEN** native fence GetCompletedValue 返回 UINT64_MAX
- **THEN** 查询报告 DeviceLost，不得按该值大于目标值而报告正常完成

### Requirement: DeviceFrameContext 显式 slot lease

DeviceFrameContext SHALL 提供显式 slot 获取、command/分配访问、提交、未提交丢弃与完成查询协议。lease SHALL 含 slot identity/generation 和 device epoch，提交或丢弃后失效。context SHALL 是 GPU slot 状态、completion 与 native 资源的唯一所有者；首版每个活动 Device SHALL 只有一个 context 和最多一个供 coordinator 建图的 Recording lease。获取 SHALL 非阻塞返回 Ready/Busy/Error，不提前 Reset 提交 fence，不自动推进隐式 frame index。

公开执行方法 SHALL 直接由 DeviceFrameContext 提供，不再通过 GetFrameExecution 或独立 compiler 接口转接。contextId SHALL 跨模块唯一，创建方 SHALL 传入非零 device epoch；不能把复用的 slot index 当成有效 lease。内部 recording ordinal SHALL 有容量上限，失败不得覆盖已录完的命令。直接宿主协议与显式协议 SHALL 在同一 context 内互斥。

#### Scenario: 旧 lease 迟到
- **WHEN** slot 已被新一轮复用或 device epoch 改变，旧 lease 继续访问分配/命令入口
- **THEN** 拒绝该访问，不操作新一轮 slot 的资源

#### Scenario: Recording 无 GPU 工作
- **WHEN** 已取得 lease 但没有任何 GPU 工作且已处理输出获取责任
- **THEN** 可丢弃未提交 recording；不 Reset fence、不制造待完成 submission，下一次 slot 获取不会等待不存在的提交

#### Scenario: 提交接受结果与 slot 状态闭合
- **WHEN** scheduler 请求 context 提交有效 recording
- **THEN** context 接管 GPU refs，在实际提交前 Reset fence，按 Accepted/Rejected/不确定错误推进 Submitted/恢复/隔离分支，不留未登记的 Reset 后失败路径

#### Scenario: 拒绝后重试或丢弃
- **WHEN** fence reset 或 queue submit 明确拒绝，尚未入队
- **THEN** 调用方仍持有 RecordedFrame，可修正后重试；销毁最后一个 recording borrower 后才能 Discard，不查询或等待这个未提交 recording 的 fence

#### Scenario: 完成通知背压
- **WHEN** completion 输出空间为零或不足以交付全部完成项
- **THEN** 未交付 submission 保留引用和 slot；每个 token 最多交付一次，正常完成交付后才可复用 slot

#### Scenario: 不确定提交或完成查询失败
- **WHEN** 无法证明是否入队，或 retained submission 的查询返回 Error/DeviceLost
- **THEN** slot 保留 GPU 引用并隔离，context 停止新 lease；错误事件只交付一次，不能用已报告错误作为提前释放证明，保留到已证明完成或设备安全 teardown

### Requirement: FrameContext allocator 分区一致

command、shader-visible descriptors、RTV/DSV、staging、dynamic UBO 与 batch pack 的 renderer 路径 SHALL 使用同一 DeviceFrameLease 对应的 slot 分区；不同 View SHALL 在其中分配不重叠 slice。后端分区数量和映射 SHALL 与配置的 inflight slots 一致，重配不得破坏在途资源。allocator SHALL 不根据独立 Tick、viewport 或隐式 AdvanceFrame 次数判断复用安全性。

#### Scenario: 非固定轮转与不同输出节拍
- **WHEN** 因空帧、失败或 viewport 暂停而重复使用某个已安全 slot
- **THEN** descriptor/staging/UBO 均选择该 lease 对应分区，不因自身计数器错位覆写其它在途 slot

#### Scenario: 未退休分区容量不足
- **WHEN** 当前 slot 分配超出预算且其它 slot 仍在途
- **THEN** 返回失败/延后或申请受同等退休约束的新 backing，不通过重置其它 slot 腾出空间

#### Scenario: 构建期复制与常量对齐
- **WHEN** coordinator 在有效 Recording lease 上写入 staging 或逐 View UBO 数据
- **THEN** 立即复制输入到该 slot 的独立范围；UBO 按设备对齐并清零 padding，返回的 size 包含 padding，失败不消耗分配游标；返回 buffer 强引用不延长 lease 或授权覆写在途范围

#### Scenario: 编译后的分配冻结
- **WHEN** 成功 Compile 已保留 plan，或 encoding/recording 仍借用 lease
- **THEN** 拒绝新的 buffer/descriptor 分配；worker 只消费提前确定的范围，不能调用 legacy current-frame 分配路径；销毁成功 plan 不自动回卷已返回分配，整槽复用必须重新获取 lease

#### Scenario: native 编译分配失败回滚
- **WHEN** compiler 在分配部分 RTV 或 footprint scratch 后失败
- **THEN** 回滚本次 compiler 内部分配游标，保留编译前的 host slice；同 lease 的合法图可重试，不能覆盖其它在途 slot

#### Scenario: MSAA 能力预检与失败重试
- **WHEN** live 图使用多采样 color 或 D32 depth storage
- **THEN** 各 backend 在 transient 分配前分别查询实际 color/depth format 与 sample 支持，Vulkan 同时按实际 usage 查询；不得用 color 能力替代 depth。不支持返回 Unsupported，原生查询失败保留 cause，同 lease 可重试；被裁剪的资源不查询或分配

#### Scenario: depth resolve 能力与 command 接口预检
- **WHEN** live 图请求 D32 depth resolve
- **THEN** Vulkan 检查实际 supportedDepthResolveModes；DX12 确认 CommandList1 与所需 programmable sample-position tier，不能用 color resolve 或 multisample quality 代替。缺能力在 transient 分配前明确 Unsupported，已有不请求该功能的图仍可用；native 错误保留 cause，同 lease 可重试。worker 不查询能力或 COM 接口

#### Scenario: depth resolve 首次目标 metadata 与退休
- **WHEN** DX12 resolve 首次写入尚未就绪的 depth target cell，或计划取消/提交
- **THEN** 在 DEPTH_WRITE 下只初始化该 cell 的 metadata，再进入 RESOLVE_DEST；已有内容不 discard，metadata 初始化不定义逻辑内容。views、descriptors、command 接口和 backing 沿现有 lease/plan 生命周期，取消先 drain，提交后等 GPU 完成或安全 teardown 才回收；回执必须计入 depth resolve 目标写入

#### Scenario: DX12 resolve 首次目标初始化
- **WHEN** resolve 或局部 copy 首次写入尚未初始化 metadata 的 render-target 子资源
- **THEN** 共享初始化路径先在合法状态初始化该 cell 的 metadata，再转换到实际写状态；不丢弃已有内容或其它 cells，不把 Discard 当作逻辑内容定义，只有真实写入范围获得内容资格

#### Scenario: attachment view 构建与退休
- **WHEN** compiler 为 mip/layer attachment 构建 native image view 或 RTV/DSV
- **THEN** 在 coordinator Compile 完成全部创建和范围解析，worker 仅消费冻结结果；同计划可共享相同 native view，DX12 descriptors 使用当前 lease 分区。失败释放已创建对象且 lease 可重试，取消等待 drain，已提交 views/heaps 保活到 GPU 完成或安全 teardown；framebuffer 先于其引用的 view 销毁

#### Scenario: descriptor 分区 reset 失败
- **WHEN** 已退休 slot 的 native descriptor pool reset 失败
- **THEN** 保留原始错误且不发布新 generation；可恢复失败允许重试，DeviceLost 关闭 context，不将重置失败误报 Ready

#### Scenario: 上传预算查询 lease 剩余容量
- **WHEN** coordinator 在 Compile 前查询 AvailableFrameBufferBytes
- **THEN** 返回对齐及设备 uniform padding 后可接纳的字节数，不分配 native buffer、不推进游标；非法 alignment、错误 context、过期或不可分配 lease 返回显式错误，容量为零不是失败

### Requirement: 后端 FrameContext 编译与 dispatch

DeviceFrameContext SHALL 提供匹配后端的完整 Compile、Dispatch、编码结果收齐和 Submit 协议。Dispatch SHALL 在该 context 管理的 worker pool/ThreadContexts 与 coordinator 编码入口执行后端 EncodeTaskGraph，默认复用 Core TaskDispatcher（RHI 保留 DeviceFrameDispatcher 兼容名）作为 worker 批次执行器，不把 GPU resource DAG 原样当 CPU DAG。coordinator-only 节点 SHALL 回到指定 coordinator，不能依赖现有 dispatcher 尚不提供的线程亲和性。serial 模式 SHALL 使用同一后端计划/结果协议；首版单 graphics queue，SubmissionPlan 可包含多个命令对象。

compiler SHALL 是各后端的私有组合实现；公共 slot 机制只能管理生命周期，不接管 native pass 构建、资源/同步计划或任务划分。尚未支持的阶段 SHALL 显式返回 Unsupported，不能调用旧公共 executor 伪造新协议成功。

#### Scenario: 三后端执行各自计划
- **WHEN** context 收到自身 Compile 生成的合法计划
- **THEN** Vulkan 可组装 secondary/primary，DX12 可收齐并排序 DIRECT lists，Metal 可创建/结束 parallel parent/children；宿主不按 API 分支录制 native passes

#### Scenario: CPU 编码完成而 GPU 未执行
- **WHEN** Dispatch 的全部任务成功并返回 RecordedFrame，但尚未 Submit
- **THEN** 没有 GPU 完成或上传 Resident 通知，没有 fence reset；scheduler 仍可按未提交路径放弃

#### Scenario: 并行路径具有真实重叠
- **WHEN** parallelNum 大于 1 且图含两个可独立录制的 native 单元
- **THEN** 可用受控同步观测不同 worker 的录制重叠；serial 与 parallel 使用各自模式的同一计划语义并得到相同读回结果，不能只依据配置值认定并行完成

#### Scenario: 有界精确 backing 状态纳管
- **WHEN** coordinator 使用 TrackResource 登记一个资源及已知初态（image 可为逐 mip/layer 完整表）
- **THEN** 验证精确 Device 归属、资源/状态类型与容量，重复登记拒绝；持有 backing 防止地址复用，首次 accepted/completed serial 均为零；资源使用只属于该 context/graphics queue，外部使用须先完成所有已纳管使用并移出

#### Scenario: 旧 slot 完成回执不能倒退账本
- **WHEN** 同一 backing 的较新 serial 先被 PollCompleted 报告，较旧 slot 随后报告
- **THEN** completed 状态与 token 不倒退；所有该 backing 的提交回执退休且无 plan/encoding/recording 借用后才允许 UntrackResource，不能让旧回执更新重新登记的条目

#### Scenario: 候选不发布与终态失效
- **WHEN** 编译或编码失败、取消、放弃、Rejected、Indeterminate 或完成查询错误
- **THEN** 未接受候选不推进 accepted/completed；Rejected 可保留同录制重试；不确定提交和终态错误使整个 context 账本不可查询或重用，保留 pins 至安全 teardown

#### Scenario: 有 owner 的账本租约自动退休
- **WHEN** TrackOwnedResource 的最后一个共享 owner 被释放
- **THEN** owner 析构不调用 context/GPU；下一次协调线程的纳管、取槽、丢弃或完成轮询入口，只在所有 plan/encoding/recording 借用和已提交使用退出后回收该条目。活跃 owner 不能经 Untrack 强制移出，失败隔离不被自动回收绕过

#### Scenario: 账本租约晚于 context 释放
- **WHEN** context 已安全退出而调用方仍保留 tracking owner，且 Device/backend module 继续存活
- **THEN** owner 不引用已销毁 context，后续释放不访问悬空 context；资源状态不能被转交给另一 context 冒充同一队列状态

#### Scenario: 精确 image cell 容量与退休
- **WHEN** coordinator 纳管 image 或安全移出已纳管 image
- **THEN** 从验证归属的 native backing 获取真实 mip/layer cell 数；未知 storage 或非法状态表拒绝。账本分别限制 backing 数与 image cell 总数，默认 1024/65,536；满额不发布条目，manual/owned 安全退休归还计数，终态不提前回收

#### Scenario: 单 cell 过期初态与在途接续
- **WHEN** 后图声明的 image initial 仅一个 mip/layer 与 accepted 状态不符
- **THEN** 整图 Compile 拒绝且 lease 可重试；从 accepted 完整快照构建的后图可在前图完成前有序 Accepted。编译/放弃不发布候选，completed 仍按完整 SubmissionToken 的 serial 单调更新

### Requirement: EncodingSession drain 与分配隔离

EncodingSession SHALL 持有 plan、任务结果、CPU borrowers 与候选 GPU leases，只有全部任务/continuations 退出且 native 编码对象收尾后才可完成或丢弃。任务 SHALL 通过显式结果报告失败并阻止依赖任务正常执行，不使用 throw/try/catch；已启动任务仍须收齐。节点预算、空图、单 worker、大 fan-out SHALL 有进展保证；worker SHALL 不阻塞等待依赖子任务或 coordinator。worker scratch/结果写入 SHALL 独立，descriptor/cache 写入 SHALL 在并行消费前冻结或有显式同步。

#### Scenario: 编译中途 descriptor 分配失败
- **WHEN** Compile 已分配部分 lease descriptor 后遇到容量或原生错误，且没有产生成功 plan
- **THEN** 回收本次未提交分配并恢复预算，不释放该 lease 此前已有的分配；清理失败显式保留原生原因和不能安全复用的占用，DeviceLost 不被次要清理错误掩盖

#### Scenario: 取消时另一个 worker 仍录制
- **WHEN** 一个编码任务失败或收到取消，另一个任务仍访问 slot
- **THEN** 不重置 allocator/arena、不复用 slot；drain 后返回失败/取消且不允许 Submit 部分结果

#### Scenario: 编码预算拒绝与重试
- **WHEN** 后端计划的分片数超过 recording 或 dispatcher 队列预算
- **THEN** 在启动 worker 前显式失败，不产出部分 RecordedFrame；正确 ticket 收齐后可释放未提交 lease，后续合法帧仍可编码

#### Scenario: 失败先于取消但其它任务未退出
- **WHEN** 某个 worker 返回 native 编码错误，另一个 worker 仍运行，随后 coordinator 请求 Cancel
- **THEN** TryCollect 在 drain 前保持 Pending，不能 reset/discard slot；收齐后保留原生错误并返回 Failed，不将已发生的失败改写为 Cancelled

#### Scenario: 多个 worker 的错误包含设备丢失
- **WHEN** 较早编号的片段报告可恢复错误，另一个片段报告 DeviceLost，且随后收到取消
- **THEN** 收齐所有工作后返回包含原生设备丢失原因的 Failed，关闭 context，禁止再取得 slot；不能仅选择最早编号错误而继续复用

#### Scenario: teardown 期间仍有 CPU 与 GPU 借用
- **WHEN** 销毁 FrameContext 时仍有编码任务或未退休提交，且调用方保持 Device/backend module 存活
- **THEN** 请求取消未启动工作并等待已启动工作退出，保留提交资源直到安全 GPU 收尾，再销毁 worker/slot pools；不能只等 CPU 编码结束便释放 GPU 使用的命令资源

#### Scenario: DX12 native list 失败后的复用
- **WHEN** DIRECT list 的 Reset 或 Close 返回失败，编码批次已收齐且未提交
- **THEN** 返回包含 HRESULT 的 Failed，不提交其它已录片段；不可复用的 native list 被丢弃，后续安全 lease 重建，仍在使用的 allocator 不提前 reset

#### Scenario: coordinator 续接 parent encoder
- **WHEN** worker 子批次完成且后续节点必须由 coordinator 结束 parent 或组装 primary
- **THEN** 通过完成通知唤醒 coordinator 执行该节点，不在 worker 假装具有 coordinator 身份；所有节点完成前不返回 RecordedFrame

### Requirement: 新帧协议的平台能力门

FrameContext 的 lease、Compile/Dispatch、受检提交与输出退休 SHALL 作为整套能力验证。已迁移 Windows Vulkan/DX12 SHALL 使用该协议；尚未实现的 Metal 或未验收宿主 SHALL 明确拒绝新入口或保留完整旧模式，不能借用旧 void 操作伪造新协议成功。新协议的后端细则仅在声明支持后适用；后续接入 SHALL 满足相同内容、失败与生命周期语义。

#### Scenario: 未实现的新后端入口
- **WHEN** 宿主请求尚未支持新帧协议的 Metal Renderer
- **THEN** 返回 Unsupported，不能创建一个表面成功但不执行的 Renderer

#### Scenario: 保留旧宿主模式
- **WHEN** 未迁移平台启动既有宿主
- **THEN** 在初始化前选择完整旧流程，不与新 Renderer 共用 Device 再建立第二套 slot 或提交循环

### Requirement: Vulkan surface query negotiation budget

Vulkan SHALL query live capabilities and native drawable extent for checked surface validation, while reusing applied format, color space and present mode on a stable swapchain. Stable prepare and post-acquire validation SHALL perform no format or present-mode enumeration. Creation, recreation and native invalidation SHALL perform complete presentation-option negotiation. This SHALL preserve the checked acquisition and presentation retirement contract. The legacy direct protocol SHALL retain full-query behavior.

#### Scenario: Stable output
- **WHEN** an unchanged swapchain successfully renders a frame without native invalidation
- **THEN** prepare and post-acquire validation each query capabilities, with zero format/mode enumeration calls and unchanged creation revision

#### Scenario: Invalidation after successful acquire or present
- **WHEN** native acquire or present reports SUBOPTIMAL
- **THEN** the next surface validation re-enumerates format/color-space/mode options while retaining the successful image acquisition or queued presentation and all synchronization responsibilities

#### Scenario: Changed or unchanged negotiation
- **WHEN** invalidated negotiation succeeds
- **THEN** while selected options differ from the applied options, validation persistently reports OutOfDate until recreation; matching options clear the dirty state without rebuilding solely because of SUBOPTIMAL

#### Scenario: Failed negotiation is retried
- **WHEN** an invalidated format/mode query fails or returns INCOMPLETE
- **THEN** explicit failure or Busy is returned without a usable snapshot, and negotiation remains dirty for retry

#### Scenario: Resize and native out-of-date
- **WHEN** live capabilities change required extent, transform or other creation constraints, or native acquisition reports OUT_OF_DATE
- **THEN** existing retirement and recreation flow is used and recreated options are selected by full enumeration; capability-query failures and minimized windows never return stale Ready
