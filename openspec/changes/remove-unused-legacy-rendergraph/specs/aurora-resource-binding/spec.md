## MODIFIED Requirements

### Requirement: Global tier 数据流

Global set 0 SHALL contain only values shared across the pipeline's views for the frame. View/camera values SHALL belong to the consuming Pass in set 1. Global uniform values SHALL use the selected shader artifact and UniformUploadPlan for checked target encoding, without inferring GPU layout from CPU sizeof. Production FrameGraph invocations SHALL retain frame-safe immutable values through the existing lease protocol.

#### Scenario: Multiple views
- **WHEN** main and shadow views use different cameras in one frame
- **THEN** they share frame-global values while retaining independent Pass camera snapshots

### Requirement: Pass tier 生命周期

Pass feature SHALL 从权威共享声明取得 CPU schema，并通过实际 shader 的 UniformUploadPlan 写入当前 lease 的 set 1 输入。不可变 shader/layout 可以共享；逐 View 数据与 native descriptors SHALL 在编译时冻结并保活至安全退休。不得依赖旧 OnSetup/OnSceneChanged 的单实例可变 RG。

#### Scenario: 同一 feature 两个 View
- **WHEN** 同一 shader/layout 为不同 View 构图
- **THEN** 共享不可变定义，各自 uniform 与 descriptor 保持独立，不互相覆写

### Requirement: Batch tier dynamic UBO

Batch set 2 SHALL 使用当前 lease 拥有的参数数据。仅 shader binding policy 显式选择 dynamic UBO 时，声明才提供对应 dynamicOffset；普通 uniform binding 不强制改为动态类型。后端 SHALL 保留声明的 base offset、逻辑 range 与 dynamicOffset，并在 Pipeline 绑定后按真实 set/binding 应用。

#### Scenario: 两个 draw 的动态窗口
- **WHEN** 两个 draws 使用相同 pack buffer 和逻辑 range，但 dynamicOffset 不同
- **THEN** 编码分别读取对应范围，顺序遵循 binding policy

#### Scenario: per-lease 不覆盖
- **WHEN** 连续帧提交不同 Batch 数据
- **THEN** 当前写入不覆盖仍在 GPU 读取的帧范围

### Requirement: FrameContext 参数分配与退休

生产 uniform 写入 SHALL 使用 DeviceFrameContext::WriteFrameBuffer 和有效 DeviceFrameLease。分配按 device 能力和请求对齐，返回 FrameBufferSlice 或显式 FrameError；预算不足不得重置已分配范围。编译计划借用或提交在途时 SHALL 禁止覆写；复用由既有 slot/lease 退休协议决定。

#### Scenario: 容量不足
- **WHEN** 同一 lease 的剩余容量不足以完成目标 ABI 写入
- **THEN** 返回明确错误，已返回 slice 的内容与偏移保持稳定

#### Scenario: 安全复用
- **WHEN** 编码、提交及引用尚未满足退休条件
- **THEN** 相应 backing 不交给新帧写入，不提供独立 Reset 旁路

### Requirement: offset 对齐取自 device capability

`DeviceCapability` SHALL 含 `minUniformBufferOffsetAlignment`（默认 256）。Vulkan / DX12 / Metal 后端 SHALL 在 `UpdateDeviceCaps()` 中从原生 limits 填真实值（Vulkan `minUniformBufferOffsetAlignment`、DX12 `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`、Metal `minConstantBufferAlignmentBytes`）。

FrameContext 的 uniform 分配 SHALL 满足 `device->GetCapability().minUniformBufferOffsetAlignment` 与 native view 对齐，MUST 不以写死的常量替代能力查询。

#### Scenario: 对齐由 caps 决定

- **WHEN** device 上报 `minUniformBufferOffsetAlignment = 16`，`Allocate(64)` 连续两次
- **THEN** 第二次返回的 offset 是 16 的倍数（而非 256 的倍数）
