# aurora-resource-name Specification

## Purpose

资源 debug 命名：`SKY_ENABLE_RESOURCE_NAME` 宏（默认 0，develop 为 1）、`Buffer/Image::Descriptor::name`、四后端 debug label 落地，使调试器显示可读资源名。

## ADDED Requirements

### Requirement: SKY_ENABLE_RESOURCE_NAME 宏默认关闭、develop 开启

`SKY_ENABLE_RESOURCE_NAME` SHALL 为一个 0/1 宏，默认 `0`；`SKY_DEVELOP == 1`（develop 构建）时 SHALL 为 `1`；未定义时 SHALL 经 `#ifndef` 兜底为 `0`。代码 SHALL 用 `#if SKY_ENABLE_RESOURCE_NAME`（而非 `#ifdef`）判读。

#### Scenario: release 默认关闭

- **WHEN** release 构建且未显式定义 `SKY_ENABLE_RESOURCE_NAME`
- **THEN** 该宏值为 `0`，资源名相关字段/调用被编译剔除

#### Scenario: develop 开启

- **WHEN** develop（`SKY_DEVELOP == 1`）构建
- **THEN** `SKY_ENABLE_RESOURCE_NAME == 1`，资源名功能编译生效

### Requirement: Buffer/Image Descriptor 提供 name 字段

`rhi::Buffer::Descriptor` 与 `rhi::Image::Descriptor` SHALL 在 `SKY_ENABLE_RESOURCE_NAME == 1` 时含 `const char *name`（默认 `nullptr`）；宏关闭时 SHALL NOT 含该字段（零开销）。

#### Scenario: develop 下 Descriptor 可设名

- **WHEN** `SKY_ENABLE_RESOURCE_NAME == 1` 时构造 `Buffer::Descriptor{... , .name = "mesh_vb"}`
- **THEN** 编译通过，`name` 指向 "mesh_vb"

### Requirement: 后端创建资源时设置 debug label

四后端 SHALL 在 `CreateBuffer` / `CreateImage` 且 `Descriptor::name` 非空时，给底层资源设置 debug label：

- Vulkan：`vkSetDebugUtilsObjectNameEXT`（`VK_EXT_debug_utils` 未启用或函数未加载时跳过）；
- DX12：`ID3D12Object::SetName`；
- Metal：`MTLResource setLabel:`；
- GLES：`glObjectLabel`（`GL_KHR_debug`）。

#### Scenario: Vulkan 资源名可被调试器读取

- **WHEN** `SKY_ENABLE_RESOURCE_NAME == 1` 且 `VK_EXT_debug_utils` 启用，用 `name="mesh_vb"` 创建 buffer
- **THEN** 该 `VkBuffer` 的 debug name 为 "mesh_vb"（调试器/RenderDoc 可见）

#### Scenario: 调试能力缺失时静默跳过

- **WHEN** 调试扩展未启用或 label 函数不可用，且 `name` 非空
- **THEN** 资源创建正常完成，label 调用被跳过，不报错

### Requirement: RenderResource 名字受宏守卫并下传

resource 层（`aurora-render-buffers`）的 `RenderResource` SHALL 仅在 `SKY_ENABLE_RESOURCE_NAME == 1` 时持有 `Name`，并在创建底层资源时把名字写入 `Descriptor::name`；宏关闭时 SHALL NOT 有 name 成员。

#### Scenario: resource 名传到后端

- **WHEN** `SKY_ENABLE_RESOURCE_NAME == 1`，创建名为 "cube_ib" 的 `IndexBuffer` 并触发惰性创建
- **THEN** 底层 `rhi::Buffer` 的 debug label 为 "cube_ib"
