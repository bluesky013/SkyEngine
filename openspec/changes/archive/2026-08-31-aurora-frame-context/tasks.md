## 1. Vulkan DeviceFrameContext

- [x] 1.1 新建 `vulkan/include/rdg/VulkanDeviceFrameContext.h`：声明 `VulkanDeviceFrameContext : public DeviceFrameContext`，成员含 `VulkanDevice* mDevice`、`std::unique_ptr<CommandPool> mPool`、`VulkanCommandPool* mVulkanPool`、`std::vector<CommandBuffer*> mBuffers`
- [x] 1.2 新建 `vulkan/src/rdg/VulkanDeviceFrameContext.cpp`：构造体调用 `device->CreateCommandPool(QueueType::GRAPHICS)`、`static_cast<VulkanCommandPool*>` 并 `Allocate()` 出 `info.inflightNum` 个 buffer；析构置空 `mPool`
- [x] 1.3 改 `vulkan/include/VulkanDevice.h`：`CreateFrameContext` override 声明改为真实返回（移除 `{ return nullptr; }` 内联）
- [x] 1.4 改 `vulkan/src/VulkanDevice.cpp`：实现 `CreateFrameContext` 返回 `new VulkanDeviceFrameContext(this, info)`

## 2. DX12 DeviceFrameContext

- [x] 2.1 新建 `dx12/include/rdg/D3D12DeviceFrameContext.h`：声明 `D3D12DeviceFrameContext : public DeviceFrameContext`，成员含 `D3D12Device* mDevice`、`std::unique_ptr<CommandPool> mPool`、`D3D12CommandPool* mD3D12Pool`、`std::vector<CommandBuffer*> mBuffers`
- [x] 2.2 新建 `dx12/src/rdg/D3D12DeviceFrameContext.cpp`：构造体调用 `device->CreateCommandPool(QueueType::GRAPHICS)`、`static_cast<D3D12CommandPool*>` 并 `Allocate()` 出 `info.inflightNum` 个 buffer；析构置空 `mPool`
- [x] 2.3 改 `dx12/include/D3D12Device.h`：补 `DeviceFrameContext* CreateFrameContext(const DeviceFrameContextInitInfo&) override;` 声明
- [x] 2.4 改 `dx12/src/D3D12Device.cpp`：实现 `CreateFrameContext` 返回 `new D3D12DeviceFrameContext(this, info)`

## 3. 构建与验证

- [x] 3.1 在 Windows 上构建 Vulkan 与 DX12 目标，确认编译链接通过
- [x] 3.2 在 Apple 上构建 Metal 目标，确认无回归（本 change 不改 Metal）
- [x] 3.3 验证 `CreateFrameContext({inflightNum=N})` 返回非空且 buffer 数 = N（对应 specs 场景）

## 4. 并行编码 (parallelNum)

- [x] 4.1 `DeviceFrameContextInitInfo` 增加 `parallelNum` 字段（默认 1）
- [x] 4.2 `VulkanCommandPool` 增加 `VkCommandBufferLevel` 参数，`Allocate()` 按 level 分配
- [x] 4.3 `VulkanDeviceFrameContext` 在 `parallelNum > 1` 时创建 `VK_COMMAND_BUFFER_LEVEL_SECONDARY` pool 并预分配 `parallelNum × inflightNum` 个 secondary buffer
- [x] 4.4 `D3D12DeviceFrameContext` 在 `parallelNum > 1` 时创建 parallel pool 并预分配 `parallelNum × inflightNum` 个 DIRECT command list
- [x] 4.5 `MetalDeviceFrameContext` 在 `parallelNum > 1` 时创建 parallel pool 并预分配 `parallelNum × inflightNum` 个 command buffer
- [x] 4.6 更新测试 `CreateFrameContext({inflightNum=3, parallelNum=2})`；Windows 构建 + 运行 Vulkan/DX12 测试通过

## 5. ThreadContext 由 FrameContext 持有

- [x] 5.1 `DeviceFrameContext` 基类增加 `mThreadPool` / `mInflightNum` / `mParallelNum`，暴露 `GetParallelContext()`
- [x] 5.2 移除 `Device::CreateAsyncContext` 虚接口，移除 `Device::GetParallelContext()` / `threadPool` / `mainContext` / `contexts`
- [x] 5.3 简化 `Device::Init()`（移除 ThreadPool / mainContext 构造），`Device::Shutdown()` 移除 mainContext 清理
- [x] 5.4 `VulkanContext` 增加 `VkCommandBufferLevel` 参数并 `pool->Init()`；`VulkanDeviceFrameContext` 用 ThreadPool factory 创建 secondary `VulkanContext` 并按 worker 分配 parallel buffer
- [x] 5.5 `D3D12DeviceFrameContext` 用 ThreadPool factory 创建 `D3D12Context` 并按 worker 分配 parallel command list
- [x] 5.6 `MetalDeviceFrameContext` 用 ThreadPool factory 创建 `MetalThreadContext`，parallel buffer 仍来自 `mParallelPool`
- [x] 5.7 更新 `aurora-rhi-conventions` spec（REMOVED Device 线程池钳制 requirement）与 `aurora/AGENTS.md` 初始化时序
