## Context

aurora 的 Vulkan 后端已启用 `VK_EXT_debug_utils`（`VulkanInstance` 创建 debug messenger，`VulkanFunctions` 加载 `vkCreateDebugUtilsMessengerEXT` / `vkDestroyDebugUtilsMessengerEXT`），但**没有**加载 `vkSetDebugUtilsObjectNameEXT`，也没有任何给资源命名的通路。DX12 / Metal / GLES 后端同样没有 debug label。

`rhi::Buffer::Descriptor` / `rhi::Image::Descriptor` 目前没有 name 字段；`RenderResource`（未来的 resource 层）有 `Name` 但没有往下传。核心 `Name`（`core/name/Name.h`）已有 `GetStr()` 返回字符串。

本 change 引入 `SKY_ENABLE_RESOURCE_NAME` 开关，把资源名从 resource 层一路传到后端 debug label，release 下零开销。

## Goals / Non-Goals

**Goals:**

- `SKY_ENABLE_RESOURCE_NAME` 宏：默认 `0`，develop 模式（Debug / editor）为 `1`，未定义兜底 `0`。
- `rhi::Buffer::Descriptor` / `rhi::Image::Descriptor` 增加 `const char *name`（`#if` 守卫）。
- 四后端在 `CreateBuffer` / `CreateImage` 时设置 debug label。
- resource 层（`aurora-render-buffers`）的 `RenderResource::name_` 受宏守卫，并传入 Descriptor。

**Non-Goals:**

- 不做资源名的运行时改名（`SetName`）与热更新。
- 不做 queue / encoder / render pass 等其它对象的命名（只做 buffer/image）。
- 不做「未启用调试扩展时强制开启扩展」——扩展未启用时 label 调用静默跳过。

## Decisions

### D1: 宏定义与默认值

```cpp
// core 侧 config 头
#ifndef SKY_ENABLE_RESOURCE_NAME
#  ifdef SKY_DEVELOP
#    define SKY_ENABLE_RESOURCE_NAME 1
#  else
#    define SKY_ENABLE_RESOURCE_NAME 0
#  endif
#endif
```

`SKY_DEVELOP` 由 `sky-develop` change 提供（构建意图，默认 0）；`SKY_ENABLE_RESOURCE_NAME` 直接挂在其上。

- **理由**：`#ifndef` 兜底使未定义 = 关闭（符合「默认为 0」）；资源名是开发者设施，应挂 `SKY_DEVELOP`。用 `#if SKY_ENABLE_RESOURCE_NAME`（而非 `#ifdef`）表达 0/1 语义。
- **备选**：直接 `#ifdef SKY_ENABLE_RESOURCE_NAME` —— 被否，无法表达「显式关」，且本开关有 0/1 两种值。

### D2: Descriptor 增加 name 字段（宏守卫）

```cpp
struct Buffer::Descriptor {
    uint64_t size = 0;
    Flags<BufferUsageFlagBit> usage;
    MemoryType memory = MemoryType::GPU_ONLY;
#if SKY_ENABLE_RESOURCE_NAME
    const char *name = nullptr;
#endif
};
```

- **理由**：release 下字段整体消失，零 ABI/内存开销；develop 下 `name` 为 C 字符串（与 `Name::GetStr()` 的 `std::string_view` 兼容，转 `const char*` 需注意生命周期，见 Open Questions）。
- **备选**：无条件保留 name 字段 —— 被否，违背「默认 0 零开销」目标。

### D3: 后端 debug label 落地

| 后端 | label 调用 |
|---|---|
| Vulkan | 加载 `vkSetDebugUtilsObjectNameEXT`，`CreateBuffer/CreateImage` 后 `VkDebugUtilsObjectNameInfoEXT` + 该函数；`VK_EXT_debug_utils` 未启用时跳过 |
| DX12 | `ID3D12Resource::SetName`（宽字符，`name` 转 UTF-16） |
| Metal | `MTLResource` `setLabel:`（`NSString`，`name` 转 UTF-8） |
| GLES | `glObjectLabel(GL_BUFFER/GL_TEXTURE, id, -1, name)`（`GL_KHR_debug`） |

- **理由**：每个后端的 debug 命名 API 不同，必须后端各自实现；label 是纯调试侧信息，未启用调试能力时静默跳过不影响正确性。
- **备选**：在接口层统一 `SetName` 虚方法 —— 被否，把 label 时序从创建时刻抽离，且增加 release 下的虚调用面。

### D4: 宏与 RenderResource 的衔接（依赖 aurora-render-buffers）

`RenderResource::name_` 改为 `#if SKY_ENABLE_RESOURCE_NAME` 守卫；`Create()` 里创建底层资源时把 `name_.GetStr()` 写入 `Descriptor::name`。resource 层在宏关闭时无 name 成员。

- **理由**：name 的引入/下传统一由宏控制，resource 层不感知后端 label 细节。
- **备选**：resource 层无条件存 name、后端决定是否用 —— 被否，release 仍要付出 name 字符串驻留/拷贝成本。

## Risks / Trade-offs

- **[Name 字符串生命周期]** `Name::GetStr()` 返回 `std::string_view`，写入 `const char *name` 需保证字符串在 `CreateBuffer/CreateImage` 调用期间有效（Name 表是驻留的，通常安全）→ 缓解：文档注明 name 指针仅在创建调用期间有效，不持有。
- **[Vulkan 函数未加载]** 旧代码未加载 `vkSetDebugUtilsObjectNameEXT` → 缓解：本 change 补加载，调用前判空。
- **[跨平台字符串编码]** DX12 `SetName` 需 UTF-16、Metal 需 UTF-8 → 缓解：各后端自行转码，接口层只传 UTF-8 源。

## Migration Plan

- 纯新增，无现有调用方。
- 回滚：删除 Descriptor name 字段与后端 label 调用即可。

## Open Questions

- develop 模式由 `SKY_DEVELOP` 表达（`sky-develop` change 已引入），`SKY_ENABLE_RESOURCE_NAME` 直接挂 `SKY_DEVELOP`。
- `Name` 到 `const char*` 的稳定获取（是否给 `Name` 增加返回 NUL 结尾 `const char*` 的接口）。
