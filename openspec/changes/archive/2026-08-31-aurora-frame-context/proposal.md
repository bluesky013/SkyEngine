## Why

`Device::CreateFrameContext()` 已在接口层声明为纯虚函数（`aurora/rhi/Device.h`），`DeviceFrameContext` 基类与 `DeviceFrameContextInitInfo{inflightNum=2}` 也已就位（`aurora/rhi/interface/include/aurora/rdg/RenderDeviceExclusive.h`）。但目前只有 **Metal** 后端提供了真实实现（`MetalDeviceFrameContext`）：它创建一个 graphics command pool，并预分配 `inflightNum` 个 command buffer 作为每帧 in-flight 缓冲。**Vulkan** 返回 `nullptr`（stub），**DX12** 完全缺省 override（纯虚未满足）。上层 `RenderDeviceExclusive` / RDG 一旦落地就无法在这两个平台工作。

## What Changes

- 新增 `VulkanDeviceFrameContext`：遵循 Metal 参考实现，创建 Vulkan graphics command pool 并预分配 `inflightNum` 个 command buffer；在 `VulkanDevice::CreateFrameContext` 中返回真实对象（替换 `return nullptr`）。
- 新增 `D3D12DeviceFrameContext`：遵循同样模式，创建 DX12 graphics command pool（command list type `DIRECT`）并预分配 `inflightNum` 个 command buffer；在 `D3D12Device` 中补上 `CreateFrameContext` override。
- `DeviceFrameContextInitInfo` 增加 `parallelNum`（默认 1），表示并行编码的 worker 线程数。
- 各后端 frame context 支持并行编码：当 `parallelNum > 1` 时构造 worker ThreadPool 并按 worker 预分配 `parallelNum × inflightNum` 个 parallel command buffer（Vulkan 每个 worker 的 `VulkanContext` 持有 `VK_COMMAND_BUFFER_LEVEL_SECONDARY` pool，DX12 每个 worker 的 `D3D12Context` 持有独立 DIRECT allocator，Metal 由 frame context 的 `MetalCommandPool` 分配）。
- 并行编码的 worker `ThreadContext` 移到 `DeviceFrameContext` 内部初始化并由其持有生命周期；`Device::CreateAsyncContext` 虚接口与 `Device::GetParallelContext()` 移除，上层改走 `DeviceFrameContext::GetParallelContext()`。
- `VulkanCommandPool` 增加 `VkCommandBufferLevel` 参数（默认 PRIMARY），用于分配 secondary command buffer。
- 文件组织对齐 Metal 的 `include/rdg/` + `src/rdg/` 目录约定（各后端 CMake 用 `GLOB_RECURSE` 自动纳入）。
- 基类 `DeviceFrameContext`（`BeginFrame` / `EndFrame` / `mFrameIndex`）与 `RenderDeviceExclusive` 的接线不在本 change 范围（属 `aurora-rdg` 渲染主循环落地，本 change 仅补齐各平台后端实现）。

## Capabilities

### New Capabilities
- `aurora-frame-context`: 各 RHI 后端（Vulkan / DX12 / Metal）的 `DeviceFrameContext` 实现，负责按 `inflightNum` 预分配每帧 in-flight command buffer，并按 `parallelNum` 预分配并行编码用的 parallel/secondary command buffer。

### Modified Capabilities
- `aurora-rhi-conventions`: REMOVED「Device 线程池容量按 capability.maxThreads 钳制」（ThreadPool 已迁至 DeviceFrameContext）。

## Impact

- **代码**：Vulkan、DX12 各新增一对 header + source（`VulkanDeviceFrameContext.{h,cpp}` / `D3D12DeviceFrameContext.{h,cpp}`），并各改一处 `*Device.{h,cpp}` 以补/改 `CreateFrameContext`；`VulkanCommandPool` 增加 level 参数；`DeviceFrameContextInitInfo` 增加 `parallelNum`；三个后端的 frame context 均持有 ThreadPool + parallel buffers；`Device` 移除 `CreateAsyncContext` / `GetParallelContext` / ThreadPool。
- **依赖**：复用现有 `CommandPool::Allocate()` / `Device::CreateCommandPool(QueueType)` 接口，无新依赖。
- **Metal**：在现有 `MetalDeviceFrameContext` 上补充 parallel pool + buffers，行为向后兼容（`parallelNum` 默认 1 时不额外分配）。
- **GLES**：当前后端目录尚未建立，不在本 change 范围（待 GLES 后端落地时以相同模式补齐）。
- **测试**：各后端构造 `CreateFrameContext({inflightNum=N, parallelNum=2})` 后断言返回非空、分配的 buffer 数量等于 N（primary）与 `parallelNum × inflightNum`（parallel）。
