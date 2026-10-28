This delta defines the final public contracts of `refactor-aurora-framegraph-contracts`. For the requirements named below it supersedes earlier API wording, including overlapping active change deltas; unmodified shader semantics, target ABI, supported capabilities and retirement requirements remain in force.

## MODIFIED Requirements

### Requirement: 各后端 Device::CreateFrameContext 返回真实实现

Device::CreateFrameContext(const DeviceFrameContextInitInfo &) SHALL 在已落地的 Vulkan、DX12 与 Metal 后端显式创建 lease-protocol DeviceFrameContext。Device::CreateDirectFrameContext(const DeviceFrameContextInitInfo &) SHALL 显式创建独立公开的 DirectFrameContext compatibility 接口。工厂 SHALL 执行可失败的初始化；合法默认配置返回相应真实实现，非法配置或初始化失败返回空指针，不以构造器抛异常报告失败。

公开类型 SHALL 不相互继承或混入对方的方法；后端内部 MAY 复用 BackendFrameContext slot storage，但协议 SHALL 在工厂创建时固定，不能由首个方法或 getter 选择。

#### Scenario: Vulkan 返回非空 frame context
- **WHEN** 在 Vulkan 后端调用 `device->CreateFrameContext({})`
- **THEN** 返回非空指针

#### Scenario: DX12 返回非空 frame context
- **WHEN** 在 DX12 后端调用 `device->CreateFrameContext({})`
- **THEN** 返回非空指针，且 `D3D12Device` 不再是抽象类（可实例化）

#### Scenario: 显式创建 direct compatibility
- **WHEN** 在已落地后端调用 CreateDirectFrameContext({})
- **THEN** 返回仅公开 TryBeginFrame、EndFrame 与 GetFrameFence 的真实 compatibility 接口，CreateFrameContext 返回的 lease 接口不再包含这些方法

### Requirement: in-flight 资源隔离

生产图的 Vulkan descriptor pool（含 retire 队列）、DX12 lease-owned shader-visible heap、uniform/storage/staging/scratch backing SHALL 属于相应 DeviceFrameLease 分区；logical group 和 native group cache 不得跨越其退休边界复用可变存储。DirectFrameContext 的 compatibility ring SHALL 由该独立协议按 retired direct slot 选择。DescriptorBatch SHALL 不再存在或参与生命周期。

#### Scenario: 帧间资源不冲突

- **WHEN** `inflightNum=2`，帧 0 与帧 1 各自持有独立的 lease-owned pool/heap/uniform-buffer 分区
- **THEN** 帧 1 的写入不覆盖帧 0 仍在 GPU 使用的资源

### Requirement: DeviceFrameContext 显式 slot lease

DeviceFrameContext SHALL 提供显式 slot 获取、command/分配访问、提交、未提交丢弃与完成查询协议。lease SHALL 含 slot identity/generation 和 device epoch，提交或丢弃后失效。context SHALL 是 GPU slot 状态、completion 与 native 资源的唯一所有者；同一生产 renderer 的所有 viewport SHALL 共用唯一 lease context，并最多持有一个供 coordinator 建图的 Recording lease。获取 SHALL 非阻塞返回 Ready/Busy/Error，不提前 Reset 提交 fence，不自动推进隐式 frame index。

公开执行方法 SHALL 直接由 DeviceFrameContext 提供，不再通过 GetFrameExecution 或独立 compiler 接口转接。contextId SHALL 跨模块唯一，创建方 SHALL 传入非零 device epoch；不能把复用的 slot index 当成有效 lease。内部 recording ordinal SHALL 有容量上限，失败不得覆盖已录完的命令。DeviceFrameContext SHALL 只公开 lease 协议，不再暴露 BeginFrame、EndFrame 或 GetFrameFence；direct compatibility SHALL 通过独立工厂创建的 DirectFrameContext 执行，不能通过 fence getter 选择或切换协议。

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

## ADDED Requirements

### Requirement: DirectFrameContext 的受检兼容协议

DirectFrameContext SHALL 仅通过 Device::CreateDirectFrameContext 显式创建。TryBeginFrame SHALL 非阻塞返回 FrameOperationComplete、FrameBusy 或 FrameError；只有当前 direct slot 的 fence 查询确认为 SIGNALED 后才可 TryReset，并 SHALL 检查查询和重置的显式原生错误。PENDING SHALL 返回 Busy 且不 reset、不推进 slot；query/reset 失败 SHALL 返回可诊断错误并停止该 context 后续使用。重复 begin 或未 begin 即 EndFrame SHALL 返回显式错误。

GetFrameFence SHALL 是纯查询，不改变协议、slot、generation 或帧进度。成功 begin 后由调用方直接提交并持有相应资源，EndFrame SHALL 完成一次配对的 slot 推进；调用方 SHALL 在销毁 compatibility context 前确保自己负责的直接提交已安全退休。该兼容接口 SHALL NOT 参与 FrameGraph 的 lease 分配、状态账本或独立 native 编译入口。

#### Scenario: 只读 fence 查询
- **WHEN** 在 direct context 上连续查询 fence，或在执行成功 begin 前后查询同一 slot
- **THEN** 查询返回该 slot 的同一 fence，不切换协议或推进 ring；lease context 的公开接口不提供该 getter

#### Scenario: Busy 和 native 错误
- **WHEN** slot fence 返回 PENDING、查询失败或 TryReset 失败
- **THEN** 分别返回 Busy 或携带 native cause 的错误；PENDING 不 reset，任何失败都不伪报 begin 成功

#### Scenario: Begin 与 End 配对
- **WHEN** 已成功 begin 后再次 begin，或尚未 begin/已 end 后再次 EndFrame
- **THEN** 显式拒绝，不重复 reset fence 或推进 slot

## REMOVED Requirements

### Requirement: DescriptorBatch 接口
**Reason**: DescriptorBatch 无生产调用者，与 numeric per-group writers 和 graph/lease-owned native materialization 重复，并错误要求以独立 frame ring 承担生产生命周期。
**Migration**: 移除 DescriptorBatch 类型、Device::CreateDescriptorBatch 及三个后端实现；跨 set 原生调用改为各 ResourceGroup::CreateEncoder() 的 numeric 写入，命名事务改为独立 CreateParameterUpdate(context)，生产图使用不可变 logical groups 和当前 lease 的后端 materializer。

#### Scenario: 旧 batch 入口移除
- **WHEN** 调用方引用 DescriptorBatch 或 Device::CreateDescriptorBatch
- **THEN** 该公开接口不可编译；迁移后的 per-group writer 保留 owner/type/range 检查，published snapshot 仍不能被 numeric writer 修改
