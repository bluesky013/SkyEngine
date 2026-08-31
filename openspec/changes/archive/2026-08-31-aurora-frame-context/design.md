## Context

`DeviceFrameContext` 是每帧 in-flight 资源的载体：上层（`RenderDeviceExclusive`，将来 `aurora-rdg` 主循环）在每个渲染帧开始/结束时调用 `BeginFrame`/`EndFrame`，并通过 `mFrameIndex` 在 N 个预分配的 command buffer 之间轮转，避免 CPU 与 GPU 抢同一块记录缓冲。

接口已就位：
- `DeviceFrameContextInitInfo{ inflightNum = 2 }` 与 `DeviceFrameContext` 基类（`mFrameIndex` + 虚 `BeginFrame`/`EndFrame`）在 `aurora/rhi/interface/include/aurora/rdg/RenderDeviceExclusive.h`。
- `Device::CreateFrameContext(const DeviceFrameContextInitInfo&)` 是纯虚函数（`aurora/rhi/Device.h`）。

当前各后端状态：

| 后端 | 状态 |
|---|---|
| Metal | ✅ `MetalDeviceFrameContext` 已实现（`metal/include/rdg/` + `metal/src/rdg/`），构造时创建 graphics command pool 并 `Allocate()` 出 `inflightNum` 个 buffer |
| Vulkan | ❌ `VulkanDevice::CreateFrameContext` 直接 `return nullptr`（stub），无 `VulkanDeviceFrameContext` 类 |
| DX12 | ❌ `D3D12Device` 未声明 `CreateFrameContext` override（纯虚未满足） |
| GLES | ⚪ 后端目录尚未建立，不在本 change 范围 |

约束：
- 三个后端 CMake 均用 `file(GLOB_RECURSE SRC_FILES src/*)` + `file(GLOB_RECURSE INC_FILES include/*)`，新建 `rdg/` 子目录文件会自动纳入编译，无需改 CMake。
- `CommandPool::Allocate()` 返回的 `CommandBuffer*` 由 pool 内部 `allocatedBuffers` 拥有并析构删除；frame context 里的 `mBuffers` 只是非拥有视图。
- `Device::CreateCommandPool(QueueType::GRAPHICS)` 已存在：Vulkan 映射到 graphics queue family 的 `VulkanCommandPool`，DX12 映射到 `D3D12_COMMAND_LIST_TYPE_DIRECT` 的 `D3D12CommandPool`。

## Goals / Non-Goals

**Goals:**
- 为 Vulkan 与 DX12 各补一个 `DeviceFrameContext` 实现，使其与 Metal 行为一致：持有 graphics command pool + `inflightNum` 个预分配 command buffer。
- 让 `VulkanDevice::CreateFrameContext` / `D3D12Device::CreateFrameContext` 返回真实对象（不再返回 nullptr / 不再缺 override）。
- 支持并行编码：`DeviceFrameContextInitInfo.parallelNum` 指定 worker 线程数，`parallelNum > 1` 时额外分配 `parallelNum × inflightNum` 个 parallel/secondary command buffer。
- 目录与命名对齐 Metal 的 `rdg/` 约定，复用现有 `Device::CreateCommandPool` + `CommandPool::Allocate` 接口，不引入新依赖。

**Non-Goals:**
- 不改动 `DeviceFrameContext` 基类 `BeginFrame`/`EndFrame`/`mFrameIndex` 语义（frame index 轮转属 `aurora-rdg` 主循环）。
- 不改动 `RenderDeviceExclusive`（其 `BeginFrame`/`EndFrame` 目前为空）。
- 不处理 multi-queue / async compute（本 change 仅 graphics command pool）。
- 不实现 GLES 后端。
- 不实现 per-frame fence/semaphore 同步（帧同步属上层 submit/present 逻辑）。

## Decisions

### 决策 1：完全复刻 Metal 的实现模式

Vulkan / DX12 的 `DeviceFrameContext` 与 `MetalDeviceFrameContext` 结构一致：

```cpp
class VulkanDeviceFrameContext : public DeviceFrameContext {
public:
    explicit VulkanDeviceFrameContext(VulkanDevice* device, const DeviceFrameContextInitInfo& info);
    ~VulkanDeviceFrameContext() noexcept override;
private:
    VulkanDevice* mDevice;
    std::unique_ptr<CommandPool> mPool;
    VulkanCommandPool* mVulkanPool = nullptr;
    std::vector<CommandBuffer*> mBuffers;
};
```

构造体：
```cpp
mPool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
mVulkanPool = static_cast<VulkanCommandPool*>(mPool.get());
mBuffers.resize(info.inflightNum);
for (uint32_t i = 0; i < info.inflightNum; ++i)
    mBuffers[i] = mVulkanPool->Allocate();
```

**Why:** 三后端行为一致，review 成本低；`CreateCommandPool` 已在 Vulkan/DX12 落地。`mPool`（`unique_ptr<CommandPool>`）拥有池，池的析构负责 `delete` 所有 buffer，frame context 析构只需置空 `mPool`。

**Alternatives considered:**
- *每个 buffer 独立 pool*：浪费 pool（Vulkan pool 可分配多个 buffer，DX12 一个 allocator 服务多个 cmdlist）；否决。
- *直接 new VulkanCommandPool 而非走 Device::CreateCommandPool*：绕过统一入口，破坏抽象；否决。

### 决策 2：文件组织 `include/rdg/` + `src/rdg/`

- Vulkan: `vulkan/include/rdg/VulkanDeviceFrameContext.h` + `vulkan/src/rdg/VulkanDeviceFrameContext.cpp`
- DX12: `dx12/include/rdg/D3D12DeviceFrameContext.h` + `dx12/src/rdg/D3D12DeviceFrameContext.cpp`

与 Metal 的 `metal/include/rdg/MetalDeviceFrameContext.h` / `metal/src/rdg/MetalDeviceFrameContext.mm` 对齐。

**Why:** `GLOB_RECURSE` 自动纳入；命名与目录一致，便于后续 `aurora-rdg` 主循环按后端查找 frame context。

### 决策 3：DX12 用 `QueueType::GRAPHICS`（→ DIRECT command list）

`D3D12Device::CreateFrameContext` 直接调 `CreateCommandPool(QueueType::GRAPHICS)`，由已有 `ToCommandListType` 映射为 `D3D12_COMMAND_LIST_TYPE_DIRECT`，`Allocate()` 已返回 `Close` 状态、可被 `Begin()` 重新 `Reset` 的 cmdlist。

**Why:** 复用现有映射，graphics frame context 用 DIRECT 语义正确；`Begin()` 已处理 allocator/cmdlist reset，无需额外逻辑。

### 决策 4：保留 `mInflightCommands` 语义但不再新增死字段

Metal 头里 `uint32_t mInflightCommands;` 是声明但未使用的死字段。Vulkan/DX12 实现只保留实际用到的成员（device / pool / 后端 pool 指针 / buffers），不复制该死字段。`inflightNum` 由 `mBuffers.size()` 表达。

**Why:** 避免在 2 个新实现中引入无意义成员；Metal 的死字段可在后续清理，不在本 change 范围。

### 决策 5：并行编码——frame context 自持 ThreadPool + per-worker buffer

`DeviceFrameContextInitInfo` 增加 `parallelNum`（默认 1）。当 `parallelNum > 1` 时，frame context 构造 worker ThreadPool 并按 worker 预分配 `parallelNum × inflightNum` 个 parallel command buffer，供 worker 线程并行录制、primary buffer 执行：

- **Vulkan**：`VulkanCommandPool` 增加 `VkCommandBufferLevel` 参数（默认 PRIMARY）；每个 worker 的 `VulkanContext` 持有 `VK_COMMAND_BUFFER_LEVEL_SECONDARY` pool，从中分配 secondary command buffer。
- **DX12**：每个 worker 的 `D3D12Context` 持有独立的 DIRECT command pool（独立 `ID3D12CommandAllocator`），支持并发录制。DX12 的 bundle（`D3D12_COMMAND_LIST_TYPE_BUNDLE`）在 `CreateCommandList` 时强制要求非空 PSO，frame context 无 PSO 可用，故并行录制用 DIRECT command list。
- **Metal**：frame context 持有额外的 `MetalCommandPool` 分配 command buffer；worker 的 `MetalThreadContext` 仅提供 autorelease pool。

**Why:** 并行编码是 RDG/renderer 主循环的刚需；DX12 一个 allocator 同一时刻只能服务一个 recording session，所以必须 per-worker pool（不能单 pool 共享）。Vulkan 的 secondary command buffer 是 `vkCmdExecuteCommands` 的标准并行录制模型。

### 决策 6：ThreadContext 由 frame context 初始化，移除 Device::CreateAsyncContext

并行编码的 worker `ThreadContext` 在 `DeviceFrameContext` 内部初始化（通过 ThreadPool 的 factory 直接 `new VulkanContext / D3D12Context / MetalThreadContext`），生命周期由 frame context 持有的 ThreadPool 管理。`Device` 不再提供 `CreateAsyncContext` 虚接口，也不暴露 `GetParallelContext()`；上层改走 `DeviceFrameContext::GetParallelContext()`。

**Why:** 并行编码 worker 是 frame context 的组成部分（其 pool 就是 per-worker parallel pool），不应由 Device 独立创建；移除 `CreateAsyncContext` 后避免了 Device 与 frame context 两套并行编码机制并存。

## Risks / Trade-offs

- [DX12 纯虚缺失意味着 `D3D12Device` 当前可能无法实例化/编译] → 本 change 补上 override 即解除；若 DX12 未参与当前构建（Windows-only），不影响其它平台构建。
- [Vulkan stub 返回 nullptr 时上层解引用会崩] → 本 change 后返回真实对象；实现后需测试 `CreateFrameContext` 返回非空。
- [buffer 所有权依赖 pool 析构顺序] → frame context 析构中 `mBuffers` 先清、`mPool` 后析构（或仅置空 `mPool` 由 unique_ptr 析构）；与 Metal 相同顺序，已验证安全。

## Migration Plan

1. 落 Vulkan：新增 `VulkanDeviceFrameContext.{h,cpp}`，改 `VulkanDevice::CreateFrameContext` 返回 `new VulkanDeviceFrameContext(this, info)`。
2. 落 DX12：新增 `D3D12DeviceFrameContext.{h,cpp}`，在 `D3D12Device.{h,cpp}` 补 `CreateFrameContext` override。
3. 构建验证：Windows 上编 Vulkan + DX12 目标；Apple 上编 Metal 确认无回归（本 change 不改 Metal）。
4. 测试：`CreateFrameContext({inflightNum=N})` 断言非空、buffer 数 = N（见 specs 场景）。

## Open Questions

- 是否需要在 frame context 中直接暴露 `GetCommandBuffer(frameIndex)` 供上层取用？当前 Metal 未暴露，`mBuffers` 为私有；本 change 保持私有，暴露接口留到 `aurora-rdg` 主循环落地时按需添加。
