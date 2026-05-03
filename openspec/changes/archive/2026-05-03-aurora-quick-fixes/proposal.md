## Why

Aurora 在 review 中发现一组**两行级、独立、零设计风险**的 bug 与 API 小问题。它们与 Submit / Barrier / ResourceGroup 等大块改动正交，可以一次性合并掉，避免长期遗留：

- `Device::Init` 里 `threadCount` 钳制读取的是默认值 1（`UpdateDeviceCaps()` 还没跑），导致线程池永远是单线程
- `Device::CreateSampler(const ResourceGroup::Descriptor&)` 是个明显 typo（应为 `CreateResourceGroup`，并且按 descriptor 类型重载本身就是 code smell）
- `Sampler::Descriptor::maxLod = 0.25f` 默认值导致 mipmap 直接采不到
- `DeviceFeature::meshShader = true` 默认值与其它特性默认 false 不一致，几乎确定是 typo
- Vulkan `VulkanEncoder::BeginRendering` 完全忽略 `DepthStencilAttachment::stencilLoadOp/stencilStoreOp`，对 D24_S8 / D32_S8 资源会丢 stencil op
- `aurora/rhi/VertexDecl.h` 用 `aurora::rhi` 命名空间，整个 aurora 其它头都是 `sky::aurora`
- `VulkanDevice::OnInit` 调用 `QueryDeviceFeatures()` 两次（一次在 `OnInit`，一次在 `CreateDevice` 内），冗余
- `aurora/core/Renderer.h/.cpp`、`aurora/rhi/interface/src/Core.cpp`（`#pragma once` 在 .cpp）、`aurora/rhi/interface/src/rdg/RenderGraph.cpp` 是空文件占位

这些问题如果不集中处理，会反复出现在后续 PR 的 review 评论里，污染 Submit / Barrier / RG 的核心讨论。

## What Changes

- **修 Device::Init clamp 顺序**：把 `threadCount = std::min(threadCount, capability.maxThreads)` 放到 `UpdateDeviceCaps()` 之后
- **重命名 `CreateSampler(ResourceGroup::Descriptor)` → `CreateResourceGroup`**（接口语义不变，只是改名；本 change 与 `aurora-resource-group` 解耦——后者会把 ResourceGroup 内容写实，但名字先在这里改）
- **修 Sampler::Descriptor 默认值**：`maxLod = 1000.f`（VK 惯例），`mipmapMode = MipFilter::LINEAR`（更符合通用预期；NEAREST 留给特殊用途显式设置）
- **修 DeviceFeature::meshShader 默认值**：改为 `false`
- **Vulkan BeginRendering 处理 stencil**：format 含 stencil aspect 时挂 `pStencilAttachment`，复用现有 `DepthStencilAttachment::stencilLoadOp/stencilStoreOp`
- **VertexDecl.h 命名空间**：改为 `sky::aurora`
- **VulkanDevice 去重 QueryDeviceFeatures**：`OnInit` 不调，仅 `CreateDevice` 内调
- **空 stub 文件**：删除 `Renderer.h/.cpp`、`Core.cpp`（位于 `interface/src/`，与同名 `Core.cpp` 内容空；保留 src 中的实际 Core.cpp）、`RenderGraph.cpp` 的纯空文件版本，或加最小 TODO 占位
- **Vulkan Encoder MAX_VB / 视口 / 剪裁的硬编码 16**：抽到 `aurora/rhi/Core.h` 常量 `MAX_VERTEX_BINDINGS` / `MAX_VIEWPORTS`，对超出 silent clamp 改为 assert（debug only）

不引入新接口、不改后端语义。所有改动都能编出绿测。

## Capabilities

### New Capabilities
- `aurora-rhi-conventions`: 接口层默认值、命名约定与一组小契约（线程池 sizing、Sampler / DeviceFeature 默认、Vulkan stencil attachment 处理、namespace 一致性）

### Modified Capabilities
（无既有 spec 受影响）

## Impact

- **接口头**：`aurora/rhi/Core.h`（+常量 + 修 Sampler 默认）、`aurora/rhi/Device.h`（重命名 CreateResourceGroup）、`aurora/rhi/VertexDecl.h`（改 namespace）
- **接口实现**：`Device.cpp`（修 Init 顺序）
- **后端**：`VulkanEncoder.cpp`（stencil + 用新常量）、`VulkanDevice.cpp`（去重 QueryDeviceFeatures + 改名）；其它后端只跟 rename 改名
- **测试**：现有测试编译应保持绿；如有 `device->CreateSampler(ResourceGroup::Descriptor{})` 的调用要改名（搜后无外部调用，仅 4 个后端 override 签名）
- **风险**：极低；本 change 是为了让后续大改动起点干净
