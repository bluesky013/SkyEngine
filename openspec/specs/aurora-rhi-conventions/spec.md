# aurora-rhi-conventions Specification

## Purpose
TBD - created by archiving change aurora-quick-fixes. Update Purpose after archive.
## Requirements
### Requirement: Device 线程池容量按 capability.maxThreads 钳制

`Device::Init()` 在构造 `ThreadPool` 之前 SHALL 已经调用过 `UpdateDeviceCaps()`，使 `capability.maxThreads` 为后端实际返回值；线程池的实际容量等于 `min(hwConcurrency - 1, capability.maxThreads)`。

#### Scenario: 后端报告 maxThreads = 8 在 16 核机器上
- **WHEN** 在 16 核机器上后端 `UpdateDeviceCaps()` 设置 `capability.maxThreads = 8`，调用 `device->Init()`
- **THEN** `device->GetParallelContext()` 返回的 ThreadPool worker 数 ≤ 8（不会被默认值 1 错误钳制）

#### Scenario: 后端报告 maxThreads = 1
- **WHEN** 后端 `UpdateDeviceCaps()` 设置 `capability.maxThreads = 1`
- **THEN** ThreadPool worker 数 = 1

### Requirement: Sampler 默认值适合通用 3D 内容

`Sampler::Descriptor` 默认值 SHALL 满足"开箱即用"的通用 3D 采样：

- `magFilter = LINEAR`
- `minFilter = LINEAR`
- `mipmapMode = LINEAR`
- `maxLod = 1000.0f`
- `minLod = 0.0f`
- 其它 wrap mode 默认 REPEAT，anisotropy 关闭

调用方 MAY 显式覆盖任意字段；本 requirement 仅约束默认值。

#### Scenario: 默认 sampler 采到 mipmap
- **WHEN** 创建带 mipmap 的纹理，使用 `Sampler::Descriptor{}` 默认值创建 sampler，远距离采样
- **THEN** 采到合适的 mip level（`maxLod=1000` 不再屏蔽 mipmap）

### Requirement: DeviceFeature 默认值统一为 false

`DeviceFeature` 结构体所有字段 SHALL 默认 `false`（包括 `meshShader`），表示"默认不假设支持"。后端在 `UpdateDeviceCaps` 时按实际能力打开。

#### Scenario: 默认构造的 DeviceFeature 全 false
- **WHEN** 创建 `DeviceFeature feat{};`
- **THEN** `feat.meshShader == false`；`feat.sparseBinding == false`；... 所有字段 false

### Requirement: Device::CreateResourceGroup 是 ResourceGroup 创建的正确入口

`Device` SHALL 提供 `CreateResourceGroup(const ResourceGroup::Descriptor &)` 接口；不再保留 `CreateSampler(const ResourceGroup::Descriptor &)` 这种基于 descriptor 类型的命名重载。

#### Scenario: 调用方按正确名字创建
- **WHEN** 调用 `device->CreateResourceGroup({...})`
- **THEN** 接口存在；返回值符合 ResourceGroup 实际实现状态（本 change 不实现内容；`aurora-resource-group` 中实现）

#### Scenario: 旧名 CreateSampler(ResourceGroup::Descriptor) 不存在
- **WHEN** 调用方尝试 `device->CreateSampler(ResourceGroup::Descriptor{})`
- **THEN** 编译错误（接口已删除该 override）

### Requirement: Vulkan dynamic rendering 处理 stencil attachment

Vulkan `BeginRendering` 实现在 `DepthStencilAttachment::image` 的 PixelFormat 包含 stencil aspect（`hasStencil == true`，如 D24_S8 / D32_S8）时 SHALL 在 `VkRenderingInfo::pStencilAttachment` 上挂等价 attachment，载入 `stencilLoadOp` / `stencilStoreOp`。

format 不含 stencil 时 SHALL 不挂 `pStencilAttachment`（保持当前行为）。

#### Scenario: D32_S8 attachment + 显式 stencil clear
- **WHEN** RenderingInfo.depthStencil.image 是 D32_S8 image，stencilLoadOp=CLEAR, stencilStoreOp=STORE, clearValue.depthStencil={1.0, 0xFF}
- **THEN** Vulkan validation layer 不报"missing stencil attachment"；shader 中读 stencil 得到 0xFF（在 LoadOp 之后）

#### Scenario: D32 attachment 不挂 stencil
- **WHEN** RenderingInfo.depthStencil.image 是 D32（无 stencil）
- **THEN** `pStencilAttachment` 为 nullptr；不报错

### Requirement: Aurora 接口 namespace 全部统一为 sky::aurora

接口层所有头文件 SHALL 使用 `namespace sky::aurora`。

#### Scenario: VertexDecl.h 使用 sky::aurora
- **WHEN** include `<aurora/rhi/VertexDecl.h>`
- **THEN** `sky::aurora::VertexDesc` 可访问；`aurora::rhi::VertexDesc` 不存在（旧 namespace 已移除）

### Requirement: Encoder 顶点 / 视口 / 剪裁绑定数量上限通过常量声明

`aurora/rhi/Core.h` SHALL 暴露：
- `MAX_VERTEX_BINDINGS`（默认 16）
- `MAX_VIEWPORTS`（默认 16）
- `MAX_COLOR_ATTACHMENTS`（已存在，默认 8）

`Encoder::BindVertexBuffers` / `SetViewport` / `SetScissor` 在 count 超过对应常量时 SHALL 在 debug build 触发 assert（不再 silent clamp）。

#### Scenario: 常量可被外部代码引用
- **WHEN** include `<aurora/rhi/Core.h>` 后 `static_cast<uint32_t>(MAX_VERTEX_BINDINGS)`
- **THEN** 编译通过，值为 16

#### Scenario: 超出 MAX_VERTEX_BINDINGS 触发 debug assert
- **WHEN** debug build 调用 `BindVertexBuffers(0, 32, views)`
- **THEN** 触发 assert；release build 行为未定义但 MUST 不静默截断为 16

