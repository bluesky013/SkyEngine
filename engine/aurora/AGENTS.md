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
2. 创建 `mainContext` 并 `OnAttach(~0u)`
3. **`UpdateDeviceCaps()`** — 后端填 `capability.maxThreads` 等
4. 用 `min(hwConcurrency-1, capability.maxThreads)` 钳制 thread pool 容量
5. 构造 ThreadPool

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

## 后续 change 路线

| Change | 状态 | 说明 |
|---|---|---|
| `aurora-quick-fixes` | ✅ 已实施 | 本文档所述默认值/命名 |
| `aurora-queue-submit-present` | 设计完成 | Queue / Submit / SwapChain Present |
| `aurora-encoder-barriers` | 设计完成 | Encoder::PipelineBarrier |
| `aurora-resource-group` | 设计完成 | ResourceGroup / PipelineLayout / 描述符绑定 |
| `aurora-renderer` | 未开 | top-level 渲染主循环 |
| `aurora-rdg` | 未开 | render graph |

详见 `openspec/changes/<name>/`。
