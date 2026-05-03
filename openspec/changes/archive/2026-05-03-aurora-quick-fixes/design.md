## Context

这是一组在 review 中识别出的"两行级"问题。每一项都已确认是 bug 或与其它默认值/惯例不一致，且都没有外部调用方依赖（aurora 还没有上层使用者）。把它们集中到一个 change 里有几个原因：

- 单独开 change 太重；分散到大 change（Submit / Barrier / RG）里又会污染主线 review
- 它们彼此正交；任何一项失败都不影响其它
- 落仓后能让后续大 change 的起点干净

约束：
- 不引入新接口
- 不破坏后端语义
- 所有现有测试编译保持绿（部分测试可能需要跟着改名一行）

## Goals / Non-Goals

**Goals:**
- 修 8 项 bug / 默认值 / 命名一致性问题
- 不留下"等以后修"的小 TODO

**Non-Goals:**
- 不做 quick-fixes 之外的任何重构
- 不修改 ResourceGroup 实质内容（只改名；实质内容是 `aurora-resource-group` change）
- 不重写 SwapChain / Submit / Barrier 缺失部分

## Decisions

### 决策 1：rename `CreateSampler(ResourceGroup::Descriptor)` → `CreateResourceGroup`

直接改名，所有 4 后端 override 一起改。即使 ResourceGroup 实质未实现，先把名字定下来；`aurora-resource-group` change 后续就直接改实现，不再动签名。

### 决策 2：Device::Init clamp 顺序——把 UpdateDeviceCaps 提前

```cpp
// 修改后：
mainContext.reset(CreateAsyncContext());
mainContext->OnAttach(~(0U));
UpdateDeviceCaps();                                         // ← 提前到这里
threadCount = std::min(threadCount, capability.maxThreads); // 现在 maxThreads 已正确
contexts.resize(threadCount);
threadPool = std::make_unique<ThreadPool>(threadCount, ...);
```

或者更彻底：把 `UpdateDeviceCaps` 移到 `OnInit` 内（让后端自己控制时序）。倾向：**前者**（最小改动）。

### 决策 3：Sampler 默认值修正

| 字段 | 当前 | 修正 |
|---|---|---|
| `magFilter` | `LINEAR` | `LINEAR`（不变） |
| `minFilter` | `LINEAR` | `LINEAR`（不变） |
| `mipmapMode` | `NEAREST` | `LINEAR` |
| `maxLod` | `0.25f` | `1000.f`（VK 惯例） |
| 其它 | — | 不变 |

**Why mipmapMode → LINEAR:** `NEAREST` mipmap 在通用 3D 内容中几乎不用；trilinear 是 sane default。如有需要可显式覆盖。

**Why maxLod → 1000:** 目前 0.25 直接屏蔽 mipmap，几乎确定是误填。

### 决策 4：DeviceFeature::meshShader 默认 false

与其它特性默认 false 一致。

### 决策 5：Vulkan BeginRendering 处理 stencil

`VulkanEncoder::BeginRendering` 在 depthStencil image format 含 stencil aspect 时增加 `pStencilAttachment`：

```cpp
VkRenderingAttachmentInfo stencilAttachment = {};
if (depthImage != nullptr && FormatHasStencil(depthImage->GetVkFormat())) {
    stencilAttachment = ...;
    stencilAttachment.loadOp  = FromLoadOp(info.depthStencil.stencilLoadOp);
    stencilAttachment.storeOp = FromStoreOp(info.depthStencil.stencilStoreOp);
    renderInfo.pStencilAttachment = &stencilAttachment;
}
```

`FormatHasStencil` 用 `GetImageFormatInfo(depthImage->GetPixelFormat()).hasStencil`（接口层已提供）。

### 决策 6：VertexDecl.h namespace 改为 sky::aurora

```cpp
// 当前：
namespace aurora::rhi {
    class VertexDesc { ... };
}

// 修改为：
namespace sky::aurora {
    class VertexDesc { ... };
}
```

VertexDesc 当前是空类，改名风险极低。

### 决策 7：去重 VulkanDevice::QueryDeviceFeatures

`OnInit` 中那次 `QueryDeviceFeatures()` 删掉，仅保留 `CreateDevice` 内的调用。

### 决策 8：MAX_VB / MAX_VIEWPORTS 抽常量

```cpp
// aurora/rhi/Core.h
static constexpr uint32_t MAX_COLOR_ATTACHMENTS  = 8;   // 已有
static constexpr uint32_t MAX_VERTEX_BINDINGS    = 16;  // 新增
static constexpr uint32_t MAX_VIEWPORTS          = 16;  // 新增
```

`VulkanGraphicsEncoder::BindVertexBuffers` / `SetViewport` / `SetScissor` 用这些常量，**且超出时 debug build assert（不再 silent clamp）**。

### 决策 9：空 stub 文件——加 TODO 还是删？

逐个决策：
- `aurora/core/Renderer.h` / `Renderer.cpp`：保留空 `namespace sky::aurora {}`，加 `// TODO(P1): renderer top-level loop, see aurora-renderer change`
- `aurora/rhi/interface/src/Core.cpp`：当前是 PixelFormat 表实现（**非空**），不动。误判项排除。
  - 备注：实际 `Core.cpp` 是 137 行 PixelFormat 表，review 时看错了；本 change 不动它
- `aurora/rhi/interface/src/rdg/RenderGraph.cpp`：保留空 `namespace`，加 `// TODO(P2): RDG, see future aurora-rdg change`

## Risks / Trade-offs

- **Sampler 默认值改动可能影响后续 dev 习惯** → 缓解：在 AGENTS.md 文档化新默认；测试不依赖默认值（都显式构造 desc）
- **Encoder silent clamp 改 assert 可能让现有测试失败** → 缓解：搜了一遍现有测试都在 ≤16 范围内，不会触发；新代码若超出会被 debug 提示
- **Vulkan stencil 处理需要 image 提供 PixelFormat 查询接口** → 当前 `VulkanImage::GetVkFormat()` 已有；新增 `GetPixelFormat()` 简单返回 `Descriptor::format`
- **rename 的 4 后端同时改，编译期可发现遗漏** → 风险极低

## Migration Plan

无外部调用方。作为单 PR 一次性合入：
1. 接口头改动（Core.h / Device.h / VertexDecl.h / Sampler 默认）
2. 接口实现（Device.cpp 改 Init）
3. 4 后端同步 rename + Vulkan stencil + Vulkan QueryDeviceFeatures 去重
4. 跑全部 `AuroraTest`

## Open Questions

- **Sampler::Descriptor::mipmapMode 默认 LINEAR 是否会让现有测试期待 NEAREST 失败？** 倾向：搜过测试，未发现依赖默认 mipmapMode 的断言；安全。
- **是否顺手统一所有头文件作者注释？**（"Created by Zach Lee" / "Created by blues" / "Created on" 三种格式 + 2022 / 2026 日期混乱） 倾向：**不在本 change 做**；这是更大范围的清理，单独开 housekeeping change 或留在 review 反馈里。
