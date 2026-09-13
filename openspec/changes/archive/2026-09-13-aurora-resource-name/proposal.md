## Why

GPU 调试定位资源很困难：RenderDoc / NSight / Xcode 里看到的是 `VkBuffer 0x…` / `ID3D12Resource 0x…` 这类无意义的句柄，无法对应到引擎里的「某块 VertexBuffer / 某张贴图」。后端原生支持给资源加 debug name（Vulkan `vkSetDebugUtilsObjectNameEXT`、DX12 `SetName`、Metal `setLabel:`、GL `glObjectLabel`），但 aurora 目前完全没有把资源名传下去，也没有任何开关控制这个非零成本的命名能力。

本 change 引入 `SKY_ENABLE_RESOURCE_NAME` 宏（默认 0，develop 模式为 1），开启后把 `RenderResource` 的 name 一路传到后端资源，让调试器能显示可读的资源名；release 下零开销（无 name 字段、无 label 调用）。

## What Changes

- **`SKY_ENABLE_RESOURCE_NAME` 宏**：默认 `0`；`_DEBUG` 定义时（Debug 构建）为 `1`；未定义时 `#ifndef` 兜底 `0`。后续 `sky-develop` change 引入 `SKY_DEVELOP` 后改由其控制。
- **`rhi::Buffer::Descriptor` / `rhi::Image::Descriptor` 增加 `const char *name`**（`#if SKY_ENABLE_RESOURCE_NAME` 守卫，release 下不存在该字段）。
- **后端在 `CreateBuffer` / `CreateImage` 时给后端资源加 debug label**：
  - Vulkan：`vkSetDebugUtilsObjectNameEXT`（补加载该函数指针）；
  - DX12：`ID3D12Object::SetName`；
  - Metal：`MTLResource` `setLabel:`；
  - GLES：`glObjectLabel`（`GL_KHR_debug`）。
- **`RenderResource::name_` 改为 `#if SKY_ENABLE_RESOURCE_NAME` 守卫**（依赖本 change；`aurora-render-buffers` 引用）。
- 测试：develop 构建下资源名正确落到底层对象（各后端 label 可查/调试器可见）。

## Capabilities

### New Capabilities

- `aurora-resource-name`: 资源 debug 命名——`SKY_ENABLE_RESOURCE_NAME` 宏（默认 0，develop 为 1）、`Buffer/Image::Descriptor::name`、四后端 debug label 落地。

### Modified Capabilities

（无既有 spec 修改）

## Impact

- **新增/修改文件**：`cmake/configuration.cmake`（或 config 头）定义宏；`aurora/rhi/Buffer.h` / `Image.h`（Descriptor 加 name）；四后端 `CreateBuffer/CreateImage` 加 label；Vulkan `VulkanFunctions` 补 `vkSetDebugUtilsObjectNameEXT`。
- **依赖**：无额外依赖；`VK_EXT_debug_utils` / `GL_KHR_debug` 等调试扩展可选启用（未启用时 label 调用跳过）。
- **调用方**：`aurora-render-buffers`（`RenderResource` name 守卫 + 传入 Descriptor）；后续 image 资源类型同样受益。
- **边界**：属于 `aurora/rhi`；resource 层仅提供 name 并下传。
