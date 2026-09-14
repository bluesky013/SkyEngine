# Aurora RHI

Aurora 是 SkyEngine 在 `dev_refactor_rhi` 分支上重写的 RHI（取代旧 `engine/rhi`）。
三个后端：Vulkan / DX12 / Metal。
**GLES 不再支持**：接口 `API` 枚举不含 `GLES`，也不提供 `AuroraGL` 后端动态库；
移动端统一走 Vulkan（Android）或 Metal（iOS），不要为 GLES 新增任何接口/后端代码。
后端以独立动态库形式编译（`AuroraVulkan` / `AuroraDX12` / `AuroraMetal`），
通过 `Instance::Init({api = ...})` 在运行时 dlopen 选择。

## 命名空间

接口与所有后端一律使用 **`namespace sky::aurora`**。
不要新增 `aurora::rhi` 或其它别名；这是 review 中确认过的规则。

## 默认值约定（quick-fixes change 设定）

| 项 | 默认 |
|---|---|
| `Sampler::Descriptor::magFilter` / `minFilter` | `Filter::LINEAR` |
| `Sampler::Descriptor::mipmapMode` | `MipFilter::LINEAR` |
| `Sampler::Descriptor::maxLod` | `1000.f`（VK 惯例，避免屏蔽 mipmap） |
| `Sampler::Descriptor::addressModeU/V/W` | `WrapMode::REPEAT` |
| `DeviceFeature::*`（包括 `meshShader`） | `false`；后端在 `UpdateDeviceCaps` 时按实际能力打开 |

## 接口层常量

定义在 `aurora/rhi/Core.h`：

- `MAX_COLOR_ATTACHMENTS = 8`
- `MAX_VERTEX_BINDINGS   = 16`
- `MAX_VIEWPORTS         = 16`

后端 Encoder 在 `BindVertexBuffers` / `SetViewport` / `SetScissor` 中
**用 `SKY_ASSERT` 检查上限，不再 silent clamp**。
超出上限是调用方 bug，不是被静默截断的 feature。

## Device 初始化时序

`Device::Init()` 顺序：

1. `OnInit(devInit)` — 后端创建 native device
2. **`UpdateDeviceCaps()`** — 后端填 `capability.maxThreads` 等

并行编码的 ThreadPool / ThreadContext 由 `DeviceFrameContext` 持有（见 `aurora-frame-context` change）：
- `DeviceFrameContextInitInfo.parallelNum` 指定 worker 线程数
- 后端 FrameContext 在 `parallelNum > 1` 时构造 ThreadPool，ThreadContext 在 FrameContext 内初始化并由 FrameContext 持有生命周期

如果你新增了 capability 字段，在 `UpdateDeviceCaps()` 里写它。

## ResourceGroup 创建

接口为 **`Device::CreateResourceGroup(const ResourceGroup::Descriptor&)`**，`Descriptor` 含 `{Shader *shader, uint32_t set}`。ResourceGroup 的 descriptor 布局从 **shader reflection** 派生，没有独立的 `ResourceGroupLayout` 对象（`aurora-remove-resource-group-layout` change 已移除）。

- `shader` MUST 非空；shader 的 `reflection` MUST 非 null（空 reflection 合法）。
- ResourceGroup 复用 shader 内派生的 native set layout（Vulkan `GetDescriptorSetLayout(set)` / DX12 从 reflection 算 descriptor 数量）。
- binding 类型统一用 `ShaderResourceType`（含 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC`，batch tier 用）；`DescriptorType` / `DescriptorBindingFlags` 已删除。

## DescriptorEncoder 批量写入

descriptor 写入统一走 **`DescriptorEncoder`**（`aurora/rhi/DescriptorEncoder.h`），`ResourceUpdateInfo` + `ResourceGroup::Update(vector)` 已移除：

- 接口：`ResourceGroup::CreateEncoder()` 返回 `std::unique_ptr<DescriptorEncoder>`；`WriteBuffer(binding, buffer, offset, range, arrayElement=0)` / `WriteImage` / `WriteSampler` + `End()` 提交。
- **Vulkan**：`VulkanResourceGroup` 持持久化 packed `mWriteInfos` + `mDirty`；`VulkanShader` 为每个 set 建 `VkDescriptorUpdateTemplate`（core 1.1，1.3 floor 下始终可用）。`End()` 仅 dirty 时 `vkUpdateDescriptorSetWithTemplate`；template 创建失败回退 `vkUpdateDescriptorSets`。DYNAMIC + `range==0` 仍 assert/warning（`aurora-dynamic-ubo-pack` 约定）。
- **DX12**：无批量 flush，descriptor heap 写入即时；动态绑定（`*_DYNAMIC`）记录到 `dynamicBindings`，bind 时走 root CBV/UAV。`End()` 是 no-op（接口对称）。
  - **shader-visible 隔离**：`D3D12DescriptorAllocator` 用 CPU-only staging heap（source of truth）+ `ringSize` 张 shader-visible heap（每 in-flight frame 一张，offset 1:1）；encoder 只写 staging，`BindResourceGroup` 前 `EnsureFrameCopy()` 用 `CopyDescriptorsSimple` 拷到当前帧 heap；`D3D12DeviceFrameContext::BeginFrame` 调 `allocator->BeginFrame(mFrameIndex)` 轮换 ring。
- **Metal**：`MetalDescriptorEncoder` 是 header-only stub，`MetalResourceGroup` 未实现，随 `aurora-resource-group Metal phase` 落地。

## DescriptorBatch 跨 set 批量

帧内跨 set 的 descriptor 写入走 **`DescriptorBatch`**（`aurora/rhi/DescriptorBatch.h`）：

- `Device::CreateDescriptorBatch()` 返回后端 batch；`WriteBuffer/WriteImage/WriteSampler(group, binding, ...)` 累积，`Flush()` 一帧一次，`Reset()` 帧末复用。
- **Vulkan** `VulkanDescriptorBatch`：累积 `VkWriteDescriptorSet`（`dstSet` = 各 group 当前 set），`Flush()` 单次 `vkUpdateDescriptorSets`。
- **DX12** `D3D12DescriptorBatch`：thin，`Write*` 复用 `D3D12DescriptorEncoder`（即时写 CPU staging），`Flush()`/`Reset()` no-op（copy 在 bind）。
- **Metal** `MetalDescriptorBatch`：stub（随 `aurora-resource-group Metal phase`）。
- 与 `DescriptorEncoder`（per-set template 路径）并存：单 set 快路径走 encoder，跨 set 批量走 batch。

## Dynamic UBO pack（batch tier / set 2）

Batch tier（set 2）用 dynamic UBO 承载 per-object uniform 数据，契约如下：

- **stable binding**：batch RG 的 dynamic UBO descriptor 每帧以 `offset=0, range=blockSize` 绑定一次（`blockSize` 为该 RG 对应 shader block 的固定大小）；帧内每 draw 只改 dynamic offset（`DrawItem::batchDynamicOffset`），不重写 descriptor。Vulkan 后端对 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC` + `bufferRange==0` 会 assert/warning，不得静默退化为 `VK_WHOLE_SIZE`。
- **`BatchAllocator` 是纯线性分配器**（`aurora/rdg/BatchAllocator.h`）：`Allocate/Write/Reset`，offset 对齐取自 `DeviceCapability::minUniformBufferOffsetAlignment`；**不感知帧**（无 frame index / slot）。
- **in-flight 安全由 `DeviceFrameContext` 维护**：每个 in-flight frame 一个 pack buffer（packed pool），帧 fence 完成后回收并 `Reset` 复用；随 `aurora-renderer` 落地，allocator 本身不实现回收/扩容。
- **batch RG 由各特性创建**（如「材质→shader」），不在 RHI/allocator 层；pipeline 层 `BatchPackWriter`（`aurora/pipeline/BatchPackWriter.h`）负责「结构体写入 → 返回 dynamic offset」。
- **注意**：`ShaderResourceType::UNIFORM_BUFFER_DYNAMIC` 当前**不**由 slang 反射产出（`ShaderCompilerSlang` 把 ParameterBlock 都映射为静态 `UNIFORM_BUFFER`）；DYNAMIC 标记由调用方 / 后续 compiler change 提供，测试中用「手工改 reflection 类型」验证 RG 语义。

## Vulkan dynamic rendering 与 stencil

`VulkanGraphicsEncoder::BeginRendering` 处理 depth + stencil：
- format 含 depth 时挂 `pDepthAttachment`
- format 含 stencil（D24_S8 / D32_S8）时挂 `pStencilAttachment`，
  load/store op 来自 `DepthStencilAttachment::stencilLoadOp/stencilStoreOp`
- `clearValue` 同时被 depth 与 stencil attachment 共享

format 的 hasDepth/hasStencil 通过 `GetImageFormatInfo(pixelFormat)` 查询。

## 平台/特性下限

- **Vulkan**：要求 1.3，`dynamicRendering` + `timelineSemaphore` 强制
- **DX12**：12.0 起步（PSO 仍是 stub；SwapChain 已落地 `D3D12SwapChain`）
- **Metal**：3 起步

## Queue / Submit / Semaphore / SwapChain（submit-present）

提交与呈现路径（`aurora-queue-submit-present` + `aurora-client-viewport`）。

### Queue::Submit 契约

- `Queue::Submit(SubmitInfo)`：`SubmitInfo` 含 `commandBuffers` / `waitSemaphores` / `signalSemaphores` / `fence`。
- `SemaphoreSubmitInfo::stageMask` 表达 wait 前 / signal 后的 pipeline stage（sync2 语义）。

### Semaphore binary vs timeline

- `SemaphoreType::BINARY`：跨 submit 的 GPU-GPU 信号（swapchain acquire → render-done → present）；值由后端隐式 +1，调用方不传 value。
- `SemaphoreType::TIMELINE`：host 与 GPU 都可 signal/wait，调用方显式传 value；跨队列同步用它。

### SwapChain 契约

- `AcquireNextImage(signalSema, fence, timeoutNs) -> index`：拿下一帧可渲染 image，signal binary sema（+ 可选 fence）。
- `Present(imageIndex, numWaitSemas, waitSemas)`：等待 render-done sema 后把 image 提交给窗口系统。
- `Resize(w, h)`：重建全部 image；旧 `GetImage` 指针全部失效。
- `GetStatus()` / `GetSurfaceSize()`：surface 尺寸由原生窗口决定，RHI 层自查；`OUT_OF_DATE` 触发 Resize，`SUBOPTIMAL` 当 `OK`，`LOST` 取消当帧 present。

### 后端差异

| 后端 | binary sema | timeline sema | Present | stageMask |
|---|---|---|---|---|
| Vulkan | `vkQueueSubmit2` binary sema | `vkSignalSemaphore` / `vkWaitSemaphores` | `vkQueuePresentKHR`（Submit 后） | 原生支持 |
| DX12 | `ID3D12Fence`（queue 级 `Wait`/`Signal`） | 同左，value 显式 | `IDXGISwapChain3::Present`（CPU wait render-done fence） | **忽略**（DX12 fence 是 queue 级，不能按 stage 阻塞） |
| Metal | `MTLSharedEvent` + 内部 binary counter | `MTLSharedEvent` + 显式 value | 每次 Present 起 fresh cmdbuf 挂 `encodeWaitForEvent` + `presentDrawable`（不与用户 Submit 链耦合） | 忽略 |

### Viewport / present 编排

- `RenderViewport`（`aurora/rdg/RenderViewport.h`）是表面基类：`Begin`/`Acquire`/`Release` + backbuffer/format/extent + acquire/render-done sema 访问器（不含 frame index/fence）。
- `ClientViewport` 持 `SwapChain` + per-viewport 帧 ring sema（本地 `mFrameSlot` 计数，非 image index、非全局 frame index）。
- inflight frame 全局化：`DeviceFrameContext` 持全局 `mFrameIndex` + N 个 fence + `GetFrameFence()`；多 viewport 一帧内共享，`SubmitInfo.fence` 用全局 fence。
- RDG 绑定 viewport：`RenderGraph::BindViewport(name, viewport)`（`ViewportImageTag`），prepare 阶段（`CullPasses` 开头）`Acquire` 解析裸 `Image*`（无 refcount，避开 swapchain image 的 `unique_ptr` 所有权冲突）。

## Barrier 用法

`PipelineBarrier` 在 **`CommandBuffer`** 上，**不**在 Encoder 上。可在以下任意时机调：
- Encoder 创建之前（典型：pass 之间 transition）
- Compute 序列中两次 Dispatch 之间
- BlitEncoder 内部之前 / 之后

### AccessFlags → ImageLayout 推导

`AccessFlagBit` 是 **stage-agnostic 的访问类别**（`SRV` / `UAV` / `CBV` / `RTV` / `DSV` / `DSV_READ` / `COPY_SRC` / `COPY_DST` / `PRESENT` / 顶点输入 / `GENERAL`），stage 由 `BarrierInfo.srcStage/dstStage` 单独表达。

`InferLayoutForAccess(AccessFlags)` 提供从 access 类别推导 canonical layout 的便利函数：

| access | layout |
|---|---|
| `RTV` | `COLOR_ATTACHMENT` |
| `DSV` | `DEPTH_STENCIL_ATTACHMENT` |
| `DSV_READ` | `DEPTH_STENCIL_READ_ONLY` |
| `SRV` | `SHADER_READ_ONLY` |
| `UAV` | `GENERAL` |
| `COPY_SRC` | `TRANSFER_SRC` |
| `COPY_DST` | `TRANSFER_DST` |
| `PRESENT` | `PRESENT` |
| 多类冲突 / 无 layout 概念 | `GENERAL` |
| `NONE` | `UNDEFINED` |

### Stage / Access 兼容性陷阱

Vulkan validation 严格检查 stage ↔ access。stage 由调用方显式填 `BarrierInfo.srcStage/dstStage`；RDG 按 pass 类型推断。常用配对：

| access | stage |
|---|---|
| `RTV` | `COLOR_OUTPUT` |
| `DSV` / `DSV_READ` | `EARLY_FRAGMENT \| LATE_FRAGMENT` |
| `SRV` / `UAV` / `CBV`（raster pass） | `VERTEX_SHADER \| FRAGMENT_SHADER` |
| `SRV` / `UAV` / `CBV`（compute pass） | `COMPUTE_SHADER` |
| `COPY_SRC` / `COPY_DST` | `TRANSFER` |
| `PRESENT` | `BOTTOM` |

### 4 后端实现位置

| 后端 | 路径 |
|---|---|
| Vulkan | `vkCmdPipelineBarrier2` 直接落 cmdbuf |
| DX12 | `ID3D12GraphicsCommandList::ResourceBarrier` 直接落 cmdlist；UAV barrier 用 null pResource |
| Metal | `MetalCommandBuffer` 内部记账 active encoder：调用时若有 → `[encoder memoryBarrierWithScope:]`；若无 → 缓存到下一次 CreateXxxEncoder 入口 flush。Layout transition 在 Metal 上 noop。Blit encoder 不暴露 memoryBarrier API（依赖 encoder 边界隐式同步） |

## RDG 用法

RDG 是 **RHI 接口的扩展**：接口层（`aurora/rhi/interface/include/aurora/rdg/` + `src/rdg/`）持有
graph 结构、setup、以及后端无关的分析（依赖边 / 拓扑 / 生命周期 / culling / transient 池），
**每个后端通过 `RDGBackend` 接口自己实现 barrier 编译器与 executor**（`CompileBarriers` / `Execute`）：

| 后端 | 实现 |
|---|---|
| Vulkan | `vulkan/src/rdg/VulkanRDGBackend.{h,cpp}` |
| DX12 | `dx12/src/rdg/D3D12RDGBackend.{h,cpp}` |
| Metal | `metal/src/rdg/MetalRDGBackend.{h,cpp}` |

创建入口：`RenderGraph::Build(device)`（`RenderGraph` 是具体类，构造时经 `Device::CreateRDGBackend()` 取后端，不搞多态图继承）。

三段式 lifecycle：

1. **Setup**：`RenderGraph::Build(device)` → `CreateTexture/CreateBuffer/Import` → `AddRasterPass/AddComputePass/AddCopyPass`（setup callback 内通过 builder 声明 `Read/Write/ColorAttachment/DepthStencilAttachment/Src/Dst`）
2. **Compile**：`Compile()` 做共享分析（依赖边 / 拓扑 / 生命周期 / culling / transient），再回调后端的 `CompileBarriers()` 做 barrier 推导
3. **Execute**：`Execute(cmdBuf)` 回调后端的 `ExecuteImpl()`（默认走共享的 `ExecutePasses` 按拓扑序 emit barrier + encoder + pass body，v1 单一 CommandBuffer）

要点：

- **handle 强类型**：`RDGTextureHandle` / `RDGBufferHandle` 是 opaque ID，不可混用；仅在所属 graph 生命周期内有效
- **execute lambda 不得手写 barrier**：所有 barrier 由 RDG 提供，execute lambda 内调用 `cmdBuf->PipelineBarrier` 是未定义行为
- **execute lambda 的 capture 生命周期**：必须在 `Execute()` 返回前保持所有 captured 引用有效
- **barrier 推导辅助在接口层**：`InferLayoutForAccess` / `InferStageForAccess`（`aurora/rhi/Barrier.h`），后端 barrier 编译器复用或覆盖
- **transient 池**：对象池（v1，按完整 desc 复用整 Image）+ 堆池（v2 预留，memory heap aliasing）
- 低复杂度场景（demo / tool）直接用 Encoder + 手写 `PipelineBarrier` 仍是合法路径

## 后续 change 路线

| Change | 状态 | 说明 |
|---|---|---|
| `aurora-quick-fixes` | ✅ 已实施 | 本文档所述默认值/命名 |
| `aurora-queue-submit-present` | ✅ 已实施 | Queue / Submit / SwapChain Present（3 后端） |
| `aurora-renderpass` | ✅ 已实施 | `FullScreenPass` 基类 + `ScenePass`(HDR) + `TextureToScreenPass`（renderpass begin/end 契约） |
| `aurora-client-viewport` | ✅ 已实施 | `RenderViewport` 表面 + `ClientViewport` + `RenderGraph::BindViewport` + inflight frame 全局化 |
| `aurora-encoder-barriers` | ✅ 已实施 | `CommandBuffer::PipelineBarrier`（最终落在 cmdbuf 而非 encoder） |
| `aurora-resource-group` | ✅ 已实施 | ResourceGroup / 描述符绑定（Vulkan + DX12；Metal / dynamic offset 留待后续） |
| `aurora-remove-resource-group-layout` | ✅ 已实施 | 移除 ResourceGroupLayout，ResourceGroup 从 shader reflection 派生 |
| `aurora-renderer` | 未开 | top-level 渲染主循环 |
| `aurora-rdg` | ✅ 已实施 | render graph（三段式 RDG） |
| `aurora-dynamic-ubo-pack` | ✅ 已实施 | Batch tier dynamic UBO stable binding + `BatchAllocator` 线性分配 + `BatchPackWriter` |

详见 `openspec/changes/<name>/`。
