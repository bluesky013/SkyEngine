# Aurora RHI

Aurora 是 SkyEngine 在 `dev_refactor_rhi` 分支上重写的 RHI（取代旧 `engine/rhi`）。
四个后端：Vulkan / DX12 / Metal / GLES。
后端以独立动态库形式编译（`AuroraVulkan` / `AuroraDX12` / `AuroraMetal` / `AuroraGL`），
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

## ResourceGroup 命名

接口为 **`Device::CreateResourceGroup(const ResourceGroup::Descriptor&)`**。
不要再用旧名 `CreateSampler(ResourceGroup::Descriptor)` —— 它在 quick-fixes change 中被重命名。
ResourceGroup 的实质实现见 `aurora-resource-group` change（仍未实现，本接口当前返回 nullptr）。

## Vulkan dynamic rendering 与 stencil

`VulkanGraphicsEncoder::BeginRendering` 处理 depth + stencil：
- format 含 depth 时挂 `pDepthAttachment`
- format 含 stencil（D24_S8 / D32_S8）时挂 `pStencilAttachment`，
  load/store op 来自 `DepthStencilAttachment::stencilLoadOp/stencilStoreOp`
- `clearValue` 同时被 depth 与 stencil attachment 共享

format 的 hasDepth/hasStencil 通过 `GetImageFormatInfo(pixelFormat)` 查询。

## 平台/特性下限

- **Vulkan**：要求 1.3，`dynamicRendering` + `timelineSemaphore` 强制
- **DX12**：12.0 起步（PSO/SwapChain 仍是 stub，见 `aurora-resource-group` change 中的 PSO 完成项）
- **Metal**：3 起步
- **GLES**：3.1 起步（compute shader 必备；多 queue 退化为单逻辑队列）

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
| GLES | 全部 access 合并 `glMemoryBarrier(...)`；layout / stage 忽略 |

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
| `aurora-queue-submit-present` | 设计完成 | Queue / Submit / SwapChain Present |
| `aurora-encoder-barriers` | ✅ 已实施 | `CommandBuffer::PipelineBarrier`（最终落在 cmdbuf 而非 encoder） |
| `aurora-resource-group` | 设计完成 | ResourceGroup / PipelineLayout / 描述符绑定 |
| `aurora-renderer` | 未开 | top-level 渲染主循环 |
| `aurora-rdg` | ✅ 已实施 | render graph（三段式 RDG） |

详见 `openspec/changes/<name>/`。
