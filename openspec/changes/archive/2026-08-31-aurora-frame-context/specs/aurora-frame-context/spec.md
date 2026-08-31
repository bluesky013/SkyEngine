## ADDED Requirements

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
