# aurora-shaders Specification

## Purpose

Define shader compilation, artifacts, caching, reflection and resource-binding ABI.

## Scope notes

### aurora-resource-binding

Define logical resource groups, checked parameter transactions, target-specific binding and uniform encoding, and the lifetime of published resource snapshots.

### aurora-shader

TBD - consolidated from: aurora-shader-derived-layout aurora-shader-derived-pso aurora-shader-derived-resource-group aurora-shader-header-codegen aurora-shader-variant shader-slang-spike

## Requirements

### Requirement: 跨后端 binding 索引一致性约定

Aurora SHALL 保证逻辑 ResourceKey 与资源语义跨后端一致；物理地址 SHALL 来自所选目标反射并允许不同。Vulkan SHALL 保留 descriptor set/binding，DX12 SHALL 保留 register class/space/register，Metal DirectArguments SHALL 保留 stage/category/index。logical set SHALL NOT 被强制解释为 Metal argument-buffer slot。调用方按逻辑键提供资源，由 Shader 派生的 BindingPlan 完成映射。

#### Scenario: 同一逻辑接口跨后端
- **WHEN** 同一语义接口使用各自目标产物绘制
- **THEN** shader 读取相同语义资源，即使各平台物理编号或 uniform 字节布局不同

#### Scenario: 同资源不同 stage 地址
- **WHEN** 一个逻辑 binding 在 Metal VS 与 FS 使用不同 index
- **THEN** 计划分别绑定两个原生地址，不重新按合并顺序编号

### Requirement: Device::CreateResourceGroup 创建 group 实例

`Device::CreateResourceGroup(const ResourceGroup::Descriptor &)` SHALL 按 `Descriptor::{shader, set}` 分配一个新的 ResourceGroup 实例。

`shader` MUST 非空；`set` SHALL 是 shader reflection 中存在的 descriptor set。返回的 ResourceGroup 在创建后所有 binding 均为"未写入"状态，必须先成功完成受检 DescriptorEncoder 事务才能被执行消费。ResourceGroup 的 binding 布局 SHALL 等于 shader reflection 中该 set 的完整资源集。

#### Scenario: 创建 group

- **WHEN** 调用 `device->CreateResourceGroup({.shader = shader, .set = 0})`
- **THEN** 返回非空 `ResourceGroup*`

#### Scenario: shader 为空拒绝

- **WHEN** `Descriptor::shader = nullptr`
- **THEN** 返回 nullptr

### Requirement: ResourceGroup::Update 批量写入资源

ResourceGroup SHALL 经同一 Name-first DescriptorEncoder 事务写入数据与资源，名称由 interface/set 参数表解析，并按计划校验类型、array element、资源类别、访问方式、device owner 与范围。错误 SHALL 通过受检结果显式报告，不能用断言、未定义行为或静默忽略表达可恢复错误。批量写入 SHALL 在全部验证与编码成功后发布不可变版本；失败 SHALL 保持先前有效绑定。多次成功事务 SHALL 允许部分更新，未覆盖的值继承先前版本，不修改仍被 GPU 使用的 descriptor/uniform。

#### Scenario: 写入 uniform 与 sampled image
- **WHEN** 对合法 uniform/image/sampler logical slots 批量提供同 device 的匹配资源
- **THEN** 发布后的绑定按计划在 native shader 中读取正确值

#### Scenario: 类型或 owner 不匹配
- **WHEN** 写入资源类型、访问方式或 GetDevice 与目标契约不匹配
- **THEN** 返回明确错误，原有有效绑定不变，不进入错误原生对象转换

### Requirement: Encoder::BindResourceGroup 真正生效，支持动态偏移

`GraphicsEncoder::BindResourceGroup(uint32_t set, ResourceGroup *group, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets)` 与 `ComputeEncoder::BindResourceGroup(...)` SHALL 把 group 绑定到当前 pipeline 的对应 set 槽。

`set` SHALL 对应当前 pipeline 经 shader 反射派生出的某个 descriptor set。

`BindResourceGroup` SHALL 绑定该 group 的当前有效不可变版本；transient/cached 策略 SHALL 由具体分配和生命周期契约选择，不从 Global/Pass/Material/Batch 名称推导。绑定前 SHALL 完成该 group 的写入提交（Vulkan `vkUpdateDescriptorSetWithTemplate` / `vkUpdateDescriptorSets`；DX12 的 staging→shader-visible copy）。

`numDynamicOffsets` MUST 等于该 group 的 layout 中 `*_DYNAMIC` 类型 binding 的总数；offsets 顺序与 binding 索引顺序一致。

#### Scenario: 绑定当前帧的 set 视图

- **WHEN** transient 模式下帧 N 更新 Global set 后 `BindResourceGroup(0, group)`
- **THEN** 绑定的是本次更新刚申请的新 set（不是上一帧的旧 set）

#### Scenario: 动态偏移

- **WHEN** layout binding 0 是 UNIFORM_BUFFER_DYNAMIC，调用 BindResourceGroup(0, group, 1, &offset0)
- **THEN** shader 读取该 UB 时基地址加上 offset0

### Requirement: Encoder::PushConstants 写入 push constants

`GraphicsEncoder::PushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data)` 与 `ComputeEncoder::PushConstants(uint32_t offset, uint32_t size, const void *data)` SHALL 把 `data` 写入当前 pipeline 的 push constants 区域。

`(offset, size)` MUST 落在 shader 反射声明的 push constant 区间内。`stages` MUST 是该区间 stage 的子集（VK 校验规则）。

#### Scenario: 写入 push constants

- **WHEN** shader 声明 push constant 区间 offset=0/size=64，调用 `PushConstants(GFX, 0, 16, data)` 后 Draw
- **THEN** shader 中 push constants 前 16 字节是 data 内容（其余字节未定义）

#### Scenario: 越界 offset+size 拒绝

- **WHEN** shader 区间是 [0, 64)，调用 `PushConstants(GFX, 60, 16, ...)`
- **THEN** Debug assert；Release 行为未定义

### Requirement: Array binding 支持 count > 1

已经声明支持 descriptor arrays 的后端/执行路径 SHALL 按 `(binding,arrayElement)` 保存和编码元素，不能把 `binding+element` 当作另一个 logical binding。count SHALL 是受检的有界数量；未实现的数组/model SHALL 明确 Unsupported，不能把零/未知 count 降为 1。uniform 字段数组及 Texture2DArray 单资源 SHALL 与 descriptor arrays 区分，本 change SHALL 保留原有受支持能力。

#### Scenario: 已支持的 sampled image descriptor 数组
- **WHEN** 支持该能力的路径向 count=8 的 binding 写入 element 0..7
- **THEN** shader 的元素 3 读取第三号元素所写纹理，且不覆盖相邻 logical binding

#### Scenario: Metal DirectArguments 未支持数组
- **WHEN** 本期 Metal 通用 Pass 路径收到 descriptor array 或未知 count
- **THEN** 在发布计划前明确 Unsupported；合法 FLOAT4 uniform 数组和 Texture2DArray 单资源不因此被拒绝

### Requirement: DescriptorEncoder 接口

DescriptorEncoder SHALL 在现有接口中统一提供受检的 Name-first WriteData、WriteBlock、WriteBuffer、WriteImage、WriteSampler 与 End。WriteData SHALL 携带中性的语义类型/shape，WriteBlock SHALL 携带 CPU schema；SHALL NOT 仅凭裸字节数推断类型。名字解析后的内部操作 SHALL 复用 BindingPlan/UniformUploadPlan，各后端 SHALL NOT 各自搜索名字。 ResourceGroup 与 Core 图装配 SHALL 直接委托同一 RHI 中性参数事务前端，统一名字、类型、重复/重叠、inactive、必需参数与原子冻结；仅原生资源和图资源的 materializer 分离。图 materializer SHALL 保留显式访问声明与现有 lease，SHALL NOT 为复用事务而提前创建尚未分配的原生图资源。接口 SHALL NOT 恢复旧 ResourceUpdateInfo，SHALL NOT include Framework、MaterialPropertyValue、Aurora.Core 图或资产类型；本次事务暂存 SHALL 使用受检的类型化数据与所有权。

#### Scenario: 混合命名批量写入
- **WHEN** 一个事务按 Name 写入材质字段、纹理和 sampler 后 End
- **THEN** 同一逻辑参数表完成类型/归属检查并原子发布，GPU 同时读到本次值与资源

#### Scenario: 同大小异类型
- **WHEN** 将 uint32_t 写入声明为 FLOAT 的名字，即使两者大小相同
- **THEN** 明确类型错误，不能 memcpy 成功

### Requirement: ResourceGroup::CreateEncoder 返回后端 encoder

ResourceGroup SHALL 从其 Shader/interface/set 创建唯一作用域的 Name-first 更新事务，并显式接受所需 RHI 中性 updateContext。上下文 SHALL 由既有帧分配/编码路径提供 uniform range 与保活能力，不暴露 AssetManager、UUID 或 GraphImageHandle。缺少必需上下文或其 lease/device epoch 失效 SHALL 明确失败。任一写入失败 SHALL 使整个事务失效，即使忽略单项结果，End 也 SHALL 返回失败，不发布合法子集。End SHALL 验证并发布参数版本而非 queue Submit；未 End 的事务销毁 SHALL 取消暂存更新。

#### Scenario: 作用域受限
- **WHEN** 向 group 的事务写入另一个 set 或接口的参数句柄
- **THEN** 返回明确错误，不跨作用域搜索或推导 slot

#### Scenario: 更新不提交 GPU
- **WHEN** Name 数据事务成功 End
- **THEN** 仅经注入上下文形成可冻结版本，不创建 queue、等待 fence 或独立 Submit

#### Scenario: 忽略单项错误
- **WHEN** 一个 Name 写入失败后调用方仍执行其它合法写入并 End
- **THEN** End 返回失败，原版本不变，不能发布后续合法子集

### Requirement: 后端原生实现

各后端 DescriptorEncoder SHALL 消费 Shader 派生的 BindingPlan。Vulkan SHALL 保留既有持久 packed descriptor、dirty 更新与 update-template/显式写入路径；DX12 SHALL 按计划选择 descriptor table 或动态 root 参数，并保留只读/可写 buffer 的 SRV/UAV 区别；Metal DirectArguments SHALL 保存受检资源并按各 stage 的原生地址编码。未实现的 argument-buffer/model SHALL 明确失败，不以空实现成功返回。

#### Scenario: Vulkan 批量提交
- **WHEN** VulkanDescriptorEncoder 完成多个合法 Write 操作并提交
- **THEN** dirty 时通过同源 template 或已验证的显式更新发布全部描述符，非 dirty 时不重复写入

#### Scenario: DX12 动态绑定
- **WHEN** 动态 uniform 或 storage buffer 写入并绑定
- **THEN** 计划明确其 CBV/SRV/UAV root 类别以及 base/range/dynamic offset，不只凭 STORAGE_BUFFER 推断 UAV

#### Scenario: Metal 直接参数
- **WHEN** 一个 ResourceGroup 被用于 graphics 或 compute
- **THEN** 只在所属活跃 stage/category/index 绑定资源，不猜 logical set 对应的物理位置

### Requirement: DX12 shader-visible descriptor ring

D3D12 后端 SHALL 用 **CPU-only staging heap**（`D3D12_DESCRIPTOR_HEAP_FLAG_NONE`）作为描述符的持久 source of truth，并用 `ringSize` 张 **shader-visible heap**（每 in-flight frame 一张，默认 3）做 GPU 可见视图，两者 offset 1:1。

`D3D12DescriptorAllocator::BeginFrame(frameIndex)` SHALL 把当前帧设为 `frameIndex % ringSize`，由 `D3D12DeviceFrameContext::BeginFrame` 调用。`D3D12ResourceGroup::EnsureFrameCopy()` SHALL 在 `mDirty` 或当前帧未复制过时，用 `CopyDescriptorsSimple` 把 staging 区间拷到当前帧的 shader-visible heap。`D3D12GraphicsEncoder::BindResourceGroup` / `D3D12ComputeEncoder::BindResourceGroup` SHALL 在绑表前调用 `EnsureFrameCopy`。

#### Scenario: 每帧重写不覆盖 in-flight 帧

- **WHEN** 帧 N 写入描述符并提交，帧 N+1 `BeginFrame(N+1)` 后重写同一 RG
- **THEN** 帧 N+1 的 copy 落到不同的 shader-visible heap，不覆盖帧 N 仍在 GPU 读取的描述符

#### Scenario: 静态描述符跨帧复用

- **WHEN** RG 写一次后连续多帧 bind
- **THEN** 每帧第一次 bind 时 `EnsureFrameCopy` 从 staging 拷到当前帧 heap，GPU 读到正确描述符

### Requirement: 移除 ResourceUpdateInfo 与 Update(vector)

`ResourceGroup` SHALL 不再有 `Update(const std::vector<ResourceUpdateInfo>&)` 方法；`ResourceUpdateInfo` 与 `ResourceWriteKind` SHALL 从接口层移除。`DescriptorHeap::Update` SHALL 同步迁移到 encoder（bindless tier2，未启用时仅签名对齐）。

#### Scenario: Update 接口移除

- **WHEN** 调用 `group->Update({write})`
- **THEN** 编译错误（接口已移除，改用 `CreateEncoder()` + `Write*` + `End()`）

### Requirement: tier 模型——tier1 pool，tier2 heap（typed 与 untyped 都走 heap）

- **tier1**（无 `VK_EXT_descriptor_heap`）：typed（`layout(set, binding)`）SHALL 走 `VkDescriptorPool` + `VkDescriptorSet`；untyped SHALL NOT 可用。
- **tier2**（有 `VK_EXT_descriptor_heap`）：typed SHALL 走 heap + `VkShaderDescriptorSetAndBindingMappingInfoEXT`，untyped SHALL 走 heap + untyped pointer（`VK_KHR_shader_untyped_pointers` + `SPV_EXT_descriptor_heap`）；**两者 SHALL 走同一套 heap**，不再用 pool。

#### Scenario: tier1 仅 typed

- **WHEN** 设备无 `VK_EXT_descriptor_heap`
- **THEN** typed `ResourceGroup` 走 pool；`CreateDescriptorHeap` 返回 nullptr

#### Scenario: tier2 typed + untyped 都走 heap

- **WHEN** 设备有 `VK_EXT_descriptor_heap`
- **THEN** typed `ResourceGroup` 与 untyped `DescriptorHeap` 都可用，都落在 heap

### Requirement: DescriptorHeap 一个对象管两个 backing buffer

`DescriptorHeap` SHALL 内部持有两个 backing buffer（resource heap + sampler heap），`Allocate` SHALL 按 per-type 索引分配，`Free` SHALL 归还，`Update` SHALL 把 descriptor 写入索引段（经 `vkWriteResourceDescriptorsEXT`）。

#### Scenario: 分配 per-type 索引

- **WHEN** 一个 material 需 2 纹理 + 1 buffer
- **THEN** `Allocate` 返回 `{texFirst, texCount=2, bufFirst, bufCount=1, ...}`，纹理与 buffer 索引各自连续

### Requirement: typed ResourceGroup 在 tier2 走 heap 映射

typed `ResourceGroup` 接口 SHALL 不变；tier2 下 backend SHALL 用 heap region + `VkShaderDescriptorSetAndBindingMappingInfoEXT` 实现，调用方 SHALL NOT 感知后端是 pool 还是 heap。

#### Scenario: typed 在 tier2 无感

- **WHEN** tier2 设备上创建并绑定 `ResourceGroup`
- **THEN** 调用方代码与 tier1 一致，backend 内部走 heap 映射

### Requirement: mapping source 先只支持 HEAP_WITH_PUSH_INDEX

heap 模型的逐 draw 索引 SHALL 先只支持 `VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT`（material index 走 push data）；`CONSTANT_OFFSET` / `INDIRECT_INDEX` / `INDIRECT_INDEX_ARRAY` / inline 系列 / shader-record SHALL 留后续。

#### Scenario: push index 逐 draw 索引

- **WHEN** bind heap 后逐 draw 经 push data 传入 index
- **THEN** shader 通过该 index 读 heap 内对应 descriptor

### Requirement: untyped 不支持 combined image sampler

`DescriptorHeap` 的 untyped 路径 SHALL NOT 支持 `COMBINED_IMAGE_SAMPLER`（combined 跨两个 heap，untyped 无法分别设 image/sampler 的 array stride），SHALL 只允许分离的 sampled image + sampler；typed 路径 SHALL 仍支持 combined。

#### Scenario: untyped 拒绝 combined

- **WHEN** 在 untyped 路径写入 combined image sampler
- **THEN** 报错/拒绝；改用分离的 image + sampler

### Requirement: descriptor heap 能力门

`DeviceFeature` SHALL 记录 `VK_EXT_descriptor_heap` 是否可用（含 `VK_KHR_shader_untyped_pointers` / `SPV_EXT_descriptor_heap` 等依赖）；`CreateDescriptorHeap` 在不可用时 SHALL 返回 nullptr，调用方退化为 tier1。

#### Scenario: 能力上报

- **WHEN** 设备支持 `VK_EXT_descriptor_heap`
- **THEN** 对应 feature 置真，`CreateDescriptorHeap` 可用；否则返回 nullptr

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

三后端 SHALL 在 `CreateBuffer` / `CreateImage` 且 `Descriptor::name` 非空时，给底层资源设置 debug label：

- Vulkan：`vkSetDebugUtilsObjectNameEXT`（`VK_EXT_debug_utils` 未启用或函数未加载时跳过）；
- DX12：`ID3D12Object::SetName`；
- Metal：`MTLResource setLabel:`。

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

### Requirement: RgBlockDesc 单一事实源

RgBlockDesc 等保留的桥接描述 SHALL 从权威共享 `.h` 的字段/资源列表派生逻辑语义，不能成为第二个可编辑 schema，也不能恢复 Slang-to-C++ 镜像生成。CPU schema SHALL 描述 C++ 表示，目标反射 SHALL 描述 GPU ABI；两侧通过 UniformUploadPlan 连接，不要求使用同一 offset 表。

#### Scenario: 同源桥接
- **WHEN** 一个共享声明同时被 C++、shader 导出与保留桥接接口消费
- **THEN** 字段与资源逻辑身份一致，原生 byte offset 仍分别由 CPU schema 与目标反射描述

#### Scenario: C++ layout 变化
- **WHEN** CPU padding/offset 改变而逻辑接口未变
- **THEN** CPU schema fingerprint 与旧 UniformUploadPlan 失效，不要求伪造目标 shader offset 或恢复镜像生成

### Requirement: Global tier 数据流

Global set 0 SHALL contain only values shared across the pipeline's views for the frame. View/camera values SHALL belong to the consuming Pass in set 1. Global uniform values SHALL use the selected shader artifact and UniformUploadPlan for checked target encoding, without inferring GPU layout from CPU sizeof. Production FrameGraph invocations SHALL retain frame-safe immutable values through the existing lease protocol.

#### Scenario: Multiple views
- **WHEN** main and shadow views use different cameras in one frame
- **THEN** they share frame-global values while retaining independent Pass camera snapshots

### Requirement: Pass tier 生命周期

Pass feature SHALL 从权威共享声明取得 CPU schema，并通过实际 shader 的 UniformUploadPlan 写入当前 lease 的 set 1 输入。不可变 shader/layout 可以共享；逐 View 数据与 native descriptors SHALL 在编译时冻结并保活至安全退休。不得依赖旧 OnSetup/OnSceneChanged 的单实例可变 RG。

#### Scenario: 同一 feature 两个 View
- **WHEN** 同一 shader/layout 为不同 View 构图
- **THEN** 共享不可变定义，各自 uniform 与 descriptor 保持独立，不互相覆写

### Requirement: Batch tier dynamic UBO

Batch set 3 SHALL 使用当前 lease 拥有的参数数据。仅 shader binding policy 显式选择 dynamic UBO 时，声明才提供对应 dynamicOffset；普通 uniform binding 不强制改为动态类型。后端 SHALL 保留声明的 base offset、逻辑 range 与 dynamicOffset，并在 Pipeline 绑定后按真实 set/binding 应用。

#### Scenario: 两个 draw 的动态窗口
- **WHEN** 两个 draws 使用相同 pack buffer 和逻辑 range，但 dynamicOffset 不同
- **THEN** 编码分别读取对应范围，顺序遵循 binding policy

#### Scenario: per-lease 不覆盖
- **WHEN** 连续帧提交不同 Batch 数据
- **THEN** 当前写入不覆盖仍在 GPU 读取的帧范围

### Requirement: dynamic UBO stable binding 契约

显式选择 dynamic UBO 的路径 SHALL 使用目标 block ABI 的固定逻辑范围，并使用 device capability 满足分配偏移及后端原生 view 对齐。该范围 SHALL NOT 统一按 std140 或 CPU sizeof 推导；原生 view 所需 padding SHALL 有足够 backing，且不能掩盖不足的逻辑范围。合法 pack buffer/布局版本不变时 SHALL 复用稳定 descriptor，逐 draw 以 dynamic offset 选择数据窗口，不要求逐对象改写 descriptor。零 range、越界或对齐错误 SHALL 显式失败，不使用断言/警告后继续或降为 whole-buffer range。

#### Scenario: 帧内稳定 descriptor
- **WHEN** 同一 pack buffer/布局版本提供多个合法 dynamic offsets
- **THEN** 各 draw 复用该版本 descriptor 并读取对应目标字节，范围与对齐来自实际目标和 device

#### Scenario: DYNAMIC 类型 range 为 0 拒绝
- **WHEN** dynamic uniform binding 提供零逻辑范围
- **THEN** 受检路径明确失败，先前有效绑定不变，不静默变成 whole-buffer view

### Requirement: FrameContext 参数分配与退休

生产 uniform/storage 写入 SHALL 使用 DeviceFrameContext::WriteFrameBuffer 和有效 DeviceFrameLease。分配按 device 能力和请求对齐，返回 FrameBufferSlice 或显式 FrameError；预算不足不得重置已分配范围。编译计划借用或提交在途时 SHALL 禁止覆写；复用由既有 slot/lease 退休协议决定。

#### Scenario: 容量不足
- **WHEN** 同一 lease 的剩余容量不足以完成目标 ABI 写入
- **THEN** 返回明确错误，已返回 slice 的内容与偏移保持稳定

#### Scenario: 安全复用
- **WHEN** 编码、提交及引用尚未满足退休条件
- **THEN** 相应 backing 不交给新帧写入，不提供独立 Reset 旁路

### Requirement: offset 对齐取自 device capability

`DeviceCapability` SHALL 含 `minUniformBufferOffsetAlignment`（默认 256）。Vulkan / DX12 / Metal 后端 SHALL 在 `UpdateDeviceCaps()` 中从原生 limits 填真实值（Vulkan `minUniformBufferOffsetAlignment`、DX12 `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`、Metal `minConstantBufferAlignmentBytes`）。

FrameContext 的 uniform 分配 SHALL 满足 `device->GetCapability().minUniformBufferOffsetAlignment` 与 native view 对齐，MUST 不以写死的常量替代能力查询。

#### Scenario: 对齐由 caps 决定

- **WHEN** device 上报 `minUniformBufferOffsetAlignment = 16`，`Allocate(64)` 连续两次
- **THEN** 第二次返回的 offset 是 16 的倍数（而非 256 的倍数）

### Requirement: pack writer 返回 dynamic offset

pipeline 的 typed uniform 写入 SHALL 先按实际 Shader 和 CPU schema 取得 UniformUploadPlan，再分配足够的目标 block 大小、编码目标字节并返回合法绑定偏移。分配对齐 SHALL 来自 device capability；逻辑绑定范围 SHALL 为实际目标范围而非 CPU sizeof 或分配 padding，native view 的额外对齐范围 SHALL 有足够 backing 且不能掩盖逻辑范围不足。动态策略 SHALL 显式选择，返回偏移 SHALL 通过既有绑定/执行路径使用。

#### Scenario: CPU 与目标大小不同
- **WHEN** 写入的 CPU 类型大小为 64 而合法目标 block 需要 80 字节
- **THEN** 分配并绑定目标所需范围，写入经计划转换的 80 字节，不复制或绑定越界

#### Scenario: Offset 直通执行
- **WHEN** 动态 uniform writer 返回的偏移被提交执行
- **THEN** executor 在相应逻辑 binding 的各原生 stage 地址使用该偏移，并校验顺序、对齐和范围

### Requirement: D3D12 tier2 bindless DescriptorHeap

`D3D12Device::CreateDescriptorHeap` SHALL 返回 `D3D12DescriptorHeap : DescriptorHeap`（而非 `nullptr`，当能力门开启时）。该对象 SHALL 内部持有两个 shader-visible backing heap（resource = `CBV_SRV_UAV`、sampler），`Allocate` SHALL 按 per-type 索引从 free list 分配并返回 `{texFirst/texCount, bufFirst/bufCount, smpFirst/smpCount}`，`Free` SHALL 归还并合并相邻区间。D3D12 descriptor 写入是即时模型：SHALL 经 `D3D12DescriptorHeap::CreateHeapEncoder(allocation)` 返回的 heap-bound `DescriptorEncoder` 立即写入；基类 `Update(allocation, encoder)` SHALL 为对称性 no-op。

#### Scenario: 分配 per-type 索引

- **WHEN** 申请 2 张纹理 + 1 个 buffer
- **THEN** `Allocate` 返回纹理索引连续、buffer 索引独立的 `Allocation`

#### Scenario: 释放后可复用

- **WHEN** `Allocate` 后 `Free` 同一 `Allocation`，再次 `Allocate` 同规模
- **THEN** 新分配成功且区间不重叠

#### Scenario: heap encoder 写入堆

- **WHEN** 用 `CreateHeapEncoder(alloc)` 的 `WriteImage/WriteBuffer/WriteSampler` 写入 descriptor
- **THEN** 对应索引段的 heap descriptor 被立即填充；`Update` 为 no-op

### Requirement: D3D12 BindDescriptorHeap 生效

`D3D12GraphicsEncoder::BindDescriptorHeap` 与 `D3D12ComputeEncoder::BindDescriptorHeap` SHALL 调用 `SetDescriptorHeaps` 绑定 heap 的 resource + sampler 两个 shader-visible heap；SHALL NOT 为空实现。

#### Scenario: 绑定 bindless heap

- **WHEN** 在 encoder 上 `BindDescriptorHeap(heap)`
- **THEN** 后续 draw/dispatch 的 shader 可经 heap index 访问 descriptor

### Requirement: D3D12 descriptor heap 能力门基于 Shader Model 6.6

`D3D12Device::UpdateDeviceCaps` SHALL 通过 `CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL)` 判定 Shader Model >= 6.6，并写入 `Device::GetFeature().descriptorHeap`。能力不可用时 `CreateDescriptorHeap` SHALL 返回 `nullptr`，调用方退化为 tier1。Vulkan 后端未实现 tier2（`VK_EXT_descriptor_heap`），`feature.descriptorHeap` SHALL 为 `false`。

#### Scenario: SM6.6 设备开启

- **WHEN** 设备支持 Shader Model 6.6
- **THEN** 设备标记 descriptor heap 可用，`CreateDescriptorHeap` 返回实例

#### Scenario: 低版本设备退化

- **WHEN** 设备最高 Shader Model < 6.6
- **THEN** `CreateDescriptorHeap` 返回 `nullptr`

### Requirement: 四层绑定与图 storage 事务完整

资源 SHALL 使用 Global 0、Pass 1、Material 2、Batch 3。逐 draw 的 SceneSurface、palette、UI 图片和 sampler SHALL 归 Batch。带 frame context 的图事务 SHALL 保留 storage 的精确 range 和显式 provider，失败 SHALL 不改变原 draw；Metal SHALL 处理全部四个 set 与 raster storage。

#### Scenario: 骨骼 palette 通过生产事务
- **WHEN** 双在途骨骼 draw 经带 context 的 Apply 绑定只读结构化 palette
- **THEN** 每个 draw 使用自己的精确范围和姿态，错误 stride 或缺失资源明确拒绝并保留原绑定

#### Scenario: 第四组与 UI
- **WHEN** raster 或 compute 使用 Batch set 3，包括 UI 逐 draw 纹理
- **THEN** 原生 materializer 保留该 set 的资源，不截断为三组，也不将逐 draw 参数放入 Global/Material

### Requirement: Uniform upload plans encode target layout

UniformUploadPlan SHALL 校验共享语义、CPU 可读范围和目标 block ABI，并编码本期受支持的 scalar/vector/MAT4/FLOAT4 数组。计划 SHALL 处理 offset、size、array stride、matrix major/stride，清零目标 padding，不读取源 padding 或修改源对象。缺失/重复/未知字段、语义冲突、越界、重叠及算术溢出 SHALL 在发布前失败。仅当完整复制契约逐字段成立、复制范围的源与目标全部由声明值字节覆盖且不含 padding 时 SHALL 使用整块复制快路；其他情况 SHALL 分段编码并清零 padding。

#### Scenario: 不同 offset 和 stride
- **WHEN** CPU 字段与目标字段位置或数组 stride 不同
- **THEN** 按目标位置编码，目标 padding 为零，源对象和边界哨兵保持不变

#### Scenario: 非对称矩阵
- **WHEN** CPU MAT4 与目标矩阵表示不同
- **THEN** 编码后 shader 对非对称矩阵的实际运算与 CPU 语义一致，不以 identity matrix 作为唯一证据

#### Scenario: 快路与通用路径等价
- **WHEN** 两条路径均适用于同一兼容 ABI 和输入值
- **THEN** 得到相同有效目标字节；总大小相同但成员布局不同不启用整块复制

### Requirement: PipelineLayout 接口对象被移除

RHI 接口层 SHALL 不暴露 `PipelineLayout` 对象：不存在 `aurora/rhi/PipelineLayout.h`，`Device` SHALL 无 `CreatePipelineLayout`，`GraphicsPipeline::Descriptor` 与 `ComputePipeline::Descriptor` SHALL 无 `layout` 字段。

#### Scenario: 无 PipelineLayout 符号

- **WHEN** 调用方 include `<aurora/rhi/PipelineLayout.h>` 或 `device->CreatePipelineLayout({})`
- **THEN** 编译失败（符号已删除）

#### Scenario: pipeline 描述符无 layout

- **WHEN** 构造 `GraphicsPipeline::Descriptor{}` / `ComputePipeline::Descriptor{}`
- **THEN** 描述符不含 layout 字段；pipeline 创建只依赖 shader

### Requirement: 后端在 Shader 内派生 native layout

各后端 SHALL 在现有 Shader 内由经过身份与完整性校验的目标 stage artifacts 派生 native layout 和 BindingPlan。Vulkan SHALL 派生 descriptor set layouts 与 VkPipelineLayout；DX12 SHALL 派生 root signature；Metal DirectArguments SHALL 派生逐 stage 的 buffer/texture/sampler 地址表。系统 SHALL NOT 恢复可由调用方独立传入的 PipelineLayout 对象，也 SHALL NOT 强制把 logical set 解释成 Metal argument-buffer slot。

#### Scenario: Pipeline 使用 shader 布局
- **WHEN** 创建 graphics 或 compute pipeline
- **THEN** pipeline 使用该 Shader 从匹配 artifacts 派生的 native layout，不接受独立布局覆盖

### Requirement: push constants 归入 shader 反射

`PipelineLayout::Descriptor::pushConstants` SHALL 被删除；push-constant 区间（offset/size/stages）SHALL 由 shader 声明并经反射得到，与 descriptor set/binding 同源。

#### Scenario: 无 PipelineLayout push constants

- **WHEN** 尝试在 layout 描述符里声明 push constants
- **THEN** 无此字段（编译失败）；push constants 改由 shader 反射承载

### Requirement: shader 派生的 set layout 供 ResourceGroup 复用

ResourceGroup SHALL 从同一 Shader 的受检 BindingPlan 获取相应 logical set 的布局。Vulkan SHALL 复用该 Shader 的真实 set 索引对应 VkDescriptorSetLayout；DX12 SHALL 使用计划中的 table/root 映射；Metal SHALL 使用计划中的逐 stage DirectArguments 地址。未实现的 binding model SHALL 返回显式 Unsupported。

#### Scenario: Vulkan set 索引有空洞
- **WHEN** Shader 的 logical sets 为 0 与 2
- **THEN** ResourceGroup 分别取得对应真实 set 的布局，不因容器下标而错位

#### Scenario: ResourceGroup 与 pipeline 同源
- **WHEN** 用同一 Shader 创建 pipeline 和 ResourceGroup
- **THEN** 两者消费同一个不可变 BindingPlan 及其 native layout

### Requirement: shader 反射进入 RHI Shader

RHI SHALL 定义不依赖 Slang 或平台原生头的逻辑接口、目标反射及 artifact 视图契约。编译器 SHALL 填充该契约；ShaderFunction SHALL 保留自身 stage artifact 的身份与不可变数据所有权；Shader SHALL 从匹配的 stage artifacts 链接反射及绑定计划。调用方 SHALL NOT 为无关代码另配一份反射绕过验证，后端 SHALL NOT 反向依赖编译器实现。

#### Scenario: 后端消费已编译产物
- **WHEN** runtime 只获得已发布 artifact，没有 Slang 库或作者源码
- **THEN** RHI 能检查并消费其中的代码、逻辑接口与目标 ABI

#### Scenario: 函数与反射错配
- **WHEN** 调用方组合属于不同 artifact 的 stage code 与 reflection
- **THEN** Shader 创建在 native 执行前明确失败

### Requirement: Vulkan layout 由反射派生

VulkanShader SHALL 从经过校验的 stage artifacts 和其链接 BindingPlan 构建 VkDescriptorSetLayout 与 VkPipelineLayout，按实际目标 descriptor set/binding、type/count/stage 可见性及已支持 push constants 派生。布局 SHALL 与实际 Shader 代码同源，不接受调用方另配未绑定 artifact 身份的反射。

#### Scenario: 反射驱动 descriptor set layout
- **WHEN** 匹配的 artifacts 包含 set 0 的 UBO 与 set 1 的 sampled image/sampler
- **THEN** VulkanShader 派生对应 set layouts 和 pipeline layout，类型、数量与实际活跃 stage 一致

### Requirement: DX12 root signature 由反射派生

D3D12Shader SHALL 从经过校验的目标反射派生 root signature，保留 register class、space、register、count 与 stage 信息；逻辑 set/binding SHALL 经 BindingPlan 映射。只读 structured/raw buffer SHALL 使用 SRV，可写变体 SHALL 使用 UAV，动态 buffer 的 root 参数选择 SHALL 同样遵守访问方式；descriptor 创建/写入 SHALL 保留 raw/structured 形态与 element stride。Graphics/Compute pipeline SHALL 使用该 Shader 的 root signature，不接受外部 root signature 参数。

#### Scenario: PSO 使用 shader 的 root signature
- **WHEN** 创建 D3D12 graphics 或 compute pipeline
- **THEN** pRootSignature 来自相应 Shader 的受检目标反射

#### Scenario: 只读输入与可写输出
- **WHEN** compute 同时声明 StructuredBuffer 或 ByteAddressBuffer 输入与对应 RW 输出
- **THEN** root/table 与 descriptor 编码分别使用 SRV 和 UAV，GPU 输出包含正确输入值

### Requirement: ResourceGroup 由 shader + set 创建

`ResourceGroup::Descriptor` SHALL 含 `Shader *shader` 与 `uint32_t set`，不再含 `ResourceGroupLayout *layout`。`Device::CreateResourceGroup(const ResourceGroup::Descriptor &)` SHALL 从 shader 反射派生该 set 的 binding 布局并分配 ResourceGroup。

`shader` MUST 非空；`set` SHALL 是 shader reflection 中存在的 descriptor set。

#### Scenario: 用 shader 创建 ResourceGroup

- **WHEN** 调用 `device->CreateResourceGroup({.shader = shader, .set = 0})`
- **THEN** 返回非空 `ResourceGroup*`，其 binding 布局与 shader reflection 的 set 0 一致

#### Scenario: shader 为空拒绝

- **WHEN** `Descriptor::shader = nullptr`
- **THEN** 返回 nullptr，且 logger 报错

### Requirement: shader reflection 的 null 与 empty 语义

Shader 创建 SHALL 要求完整、非缺失的 stage artifact 与目标反射。显式标为完整且活跃资源集合为空的反射 SHALL 合法；缺失反射、未知完整性或错误 stage SHALL 明确失败。零资源 SHALL NOT 表示顺序槽位兼容模式；不存在的 logical set SHALL 不能创建 ResourceGroup。

#### Scenario: 缺失反射拒绝
- **WHEN** stage code 没有匹配的完整目标反射
- **THEN** Shader 创建失败，不按资源顺序补出地址

#### Scenario: 零资源合法
- **WHEN** 合法 artifact 显式携带完整的零资源反射
- **THEN** Shader 可创建，针对不存在 set 的 ResourceGroup 创建失败

### Requirement: ResourceGroup 复用 shader 派生的 native set layout

各后端 ResourceGroup SHALL 复用同一 Shader 内已派生的 BindingPlan 与 native layout，不自建另一套映射。Vulkan SHALL 用对应 set 的 VkDescriptorSetLayout 分配；DX12 SHALL 按计划分配并编码 table/root 资源；Metal DirectArguments SHALL 按计划保存并绑定逐 stage 参数。其所有权 SHALL 保证 ResourceGroup 使用期间计划与 native layout 有效。

#### Scenario: Vulkan descriptor 与 pipeline 同源
- **WHEN** 使用同一 Shader 的 ResourceGroup 执行绘制
- **THEN** descriptor set 与 pipeline 使用同源 VkDescriptorSetLayout

#### Scenario: Metal stage 地址不同
- **WHEN** 一个逻辑 uniform 分别映射为 VS buffer(2) 与 FS buffer(5)
- **THEN** ResourceGroup 在两个 stage 使用各自记录的地址与本次 draw 的数据偏移

### Requirement: ResourceGroup 布局等于 shader reflection 的完整 set

ResourceGroup SHALL cover the selected Shader's complete active logical set through its BindingPlan. Name-first transactions SHALL validate all required active resources/fields before publishing the first complete parameter version. Subsequent partial updates SHALL inherit prior values. Missing required inputs SHALL NOT be treated as valid null descriptors; declared inactive keys SHALL remain distinguishable from unknown names.

#### Scenario: Required binding missing
- **WHEN** the first transaction supplies only a uniform while an active image is also required
- **THEN** End fails without publishing a usable partial version

#### Scenario: Later partial update
- **WHEN** an already complete group updates only its uniform
- **THEN** the new version retains the previous image while old in-flight versions remain unchanged

### Requirement: shader 生命周期不因 ResourceGroup 延长

Shader-derived native layout and binding plans SHALL remain valid while pipelines, ResourceGroups or frozen parameter snapshots use them. Releasing the caller's Shader reference SHALL NOT invalidate these borrowers. Shared ownership MAY retain the Shader or an independently owned immutable layout; no caller-managed delayed release SHALL be required. Frame/GPU resource lifetime SHALL still follow the normal retirement protocol.

#### Scenario: Caller releases Shader
- **WHEN** a pipeline and ResourceGroup remain after the caller releases its Shader reference
- **THEN** their layout and parameter mappings remain usable until their final borrower retires

### Requirement: UBO 类型按显式绑定声明

Shared declarations SHALL describe the base uniform type. The consuming Pass/material SHALL explicitly select static or dynamic binding and keep shader/layout type, allocation and binding offsets consistent; set numbers SHALL NOT determine this policy.

#### Scenario: Explicit dynamic binding
- **WHEN** a consumer selects a dynamic uniform binding
- **THEN** its shader layout, allocation and supplied offsets agree; a policy mismatch returns an explicit error before drawing

### Requirement: Pass-owned shared header source

每个采用共享声明的 Global/Pass/Material/Batch 接口 SHALL 有一个权威共享 `.h`，直接供 C++ 消费，并由既有工具导出持久、版本化 `.slangh` 供 shader 消费。相同字段/资源列表 SHALL 产生语义接口、typed logical slots 与 CPU schema；CPU sizeof/offsetof SHALL 仅描述 CPU 表示，不要求等于所有目标 ABI。Global/Pass/Material/Batch SHALL 分别使用 logical sets 0/1/2/3，View SHALL 按需归 Pass。

#### Scenario: C++ 无 shader 生成前置
- **WHEN** 编译消费共享声明的 C++ target
- **THEN** 无需执行 Slang、ShaderHeaderTool 或目标平台生成器即可取得 C++ 类型和 CPU schema

#### Scenario: 共享源变更
- **WHEN** 修改权威声明或其导出 helper
- **THEN** 相关 C++、持久 shader export 与 shader 依赖身份按各自输入更新，不生成供 C++ 消费的镜像 ABI header

#### Scenario: 目标布局不同
- **WHEN** 同一平坦 uniform 接口的 CPU 与 SPIR-V、DXIL 或 MSL 目标布局不同
- **THEN** 语义兼容的受支持字段通过 UniformUploadPlan 编码，不因合法 offset/stride 差异而拒绝

### Requirement: 虚拟 include

`ShaderCompilerSlang` SHALL resolve quoted and angle includes through `ShaderFileSystem`, including persisted shader-only .slangh exports and their common helper, without requiring disk copies or native host header generators.

#### Scenario: Packaged scene shaders
- **WHEN** only the built-in scene virtual files are registered, without filesystem search paths
- **THEN** SceneForward, SceneLit, SceneShadow and SceneToneMap compile for each supported target with their shared declaration includes

#### Scenario: Shared helper cache invalidation
- **WHEN** a shared helper reached through an angle include changes
- **THEN** the source closure hash changes and old cache entries are rejected; quoted includes search the including directory before the logical root

### Requirement: 类型映射

共享声明 SHALL 支持 FLOAT/INT/UINT、FLOAT2/FLOAT4、MAT4 和有界 FLOAT4 uniform 数组，保留逻辑字段类型、元素数量及 CPU 表示。目标反射 SHALL 独立描述其实际存储布局和矩阵表示。BOOL/FLOAT3/half、嵌套结构、未支持矩阵或数组、未支持的 storage-buffer 形态与 descriptor arrays SHALL 明确失败，不能遗漏字段。共享声明 SHALL 支持用于 palette 的只读 STRUCTURED/FLOAT4 buffer，并保留访问方式及 element stride。资源-only 声明 SHALL 保留形状及 typed slots，不引入占位 UBO；描述一种资源 SHALL NOT 意味着该执行路径已支持它。

#### Scenario: 有界 uniform 数组
- **WHEN** 共享接口声明 FLOAT4 数组
- **THEN** CPU 与目标各自记录 count/stride，上传计划按元素编码；合法的 stride 差异不要求两侧原始字节相同

#### Scenario: Resource-only 声明
- **WHEN** 接口只包含受支持的 texture/sampler 资源
- **THEN** 产生资源语义与 typed slots，不产生占位常量块

#### Scenario: 不支持字段拒绝
- **WHEN** 声明包含本期不支持的字段类型
- **THEN** 导出或验证明确失败，不能发布缺字段的接口或计划

### Requirement: 三方一致性校验

系统 SHALL 分别验证共享语义、CPU schema 与每个目标 ABI，验证上传后的目标字节与语义值一致。CPU schema SHALL 使用真实 sizeof/offsetof；目标 layout SHALL 来自请求目标自身；RgBlockDesc 等桥接描述 SHALL 继续从同一字段列表派生，不成为独立事实源。合法布局差异 SHALL 由 UniformUploadPlan 处理，语义冲突或不能编码的布局 SHALL 在修改 draw/执行 GPU 前明确失败。各目标编译反射测试 SHALL NOT 代替对应平台原生执行。

#### Scenario: 语义漂移被拒绝
- **WHEN** 目标字段的逻辑类型、数量或含义与共享接口不兼容
- **THEN** 计划构建或装配明确失败，原 draw 不变

#### Scenario: 不同 packing 保持值一致
- **WHEN** CPU schema 与目标 ABI 的 offset、数组 stride 或矩阵表示不同且处于支持集合
- **THEN** 编码后 shader 读到与 CPU 语义相同的值

#### Scenario: 交叉构建
- **WHEN** 移动目标编译共享 C++ 声明
- **THEN** 无需执行目标生成器；交叉构建结果与真机 shader 执行证据分开记录

### Requirement: 统一变体模型

Aurora SHALL normalize one typed key/value selection from Vertex, Pipeline and Shader-local inputs. Material, custom Batch, Fullscreen and Compute callers SHALL provide the Shader-local domain without requiring a Material object. Each logical key SHALL have one owner and a consistent type/range/default across stages. Unknown, duplicate, wrong-owner and invalid values SHALL fail explicitly. Omitted and explicit defaults SHALL normalize identically. Stage projection SHALL preserve only that stage's declared dependencies.

#### Scenario: Pass without Mesh
- **WHEN** fullscreen or compute supplies Pipeline and Shader-local inputs without Mesh input
- **THEN** it uses the same selector and validation as a scene request

#### Scenario: Duplicate or cross-domain input
- **WHEN** providers repeat a key or write a key owned by another domain
- **THEN** selection fails without order-dependent overriding

### Requirement: bitmask key 与数据驱动 schema

ShaderVariantSchema SHALL describe typed logical keys, ownership, defaults and ranges independently of the legacy 128-bit ShaderVariantKey. Full selections SHALL use canonical typed key/value identities and support the schema's checked limits (1024 entries and 32768 logical bits). The retained bitmask helper SHALL reject values or layouts it cannot represent; it SHALL NOT limit production selection to 128 bits or truncate signed values.

#### Scenario: Complete selection exceeds the legacy key
- **WHEN** a valid typed selection needs more than 128 bits
- **THEN** normalization and artifact lookup preserve every value without requiring the legacy bitmask helper

#### Scenario: Legacy helper cannot represent the selection
- **WHEN** a caller explicitly uses ShaderVariantKey with an unsupported width
- **THEN** it reports failure instead of truncating the logical selection

### Requirement: 全局 pipeline 位预留

`GlobalVariantLayout` SHALL 定义引擎级 pipeline 位预留区（固定 bit 位置），所有 shader schema 的 per-shader 位 SHALL 排在预留区之后；Pipeline 的变体组装 SHALL 只用 `GlobalVariantLayout` 设 pipeline 位，不依赖具体 shader schema。

#### Scenario: pipeline 位跨 shader 一致

- **WHEN** 两个不同 shader 的 schema 都引用同一 `GlobalVariantLayout`
- **THEN** `"shadows"` 在两个 shader 里映射到相同 bit 位

### Requirement: 机制由 shader 声明决定

Business inputs SHALL NOT select macro/native policy or native IDs. JSON companions SHALL retain logical key metadata and any explicit source-symbol association. The compiler SHALL derive target implementation and native mappings from validated shader declarations and target support. Supported macro or equivalent compile-time paths SHALL consume the selected values; native paths SHALL retain their stage/type/ID mapping. Unsupported conversion SHALL fail rather than becoming a uniform or using a default.

#### Scenario: One key across targets
- **WHEN** a shader key uses native specialization on a supported target and an explicitly supported compile-time path on another
- **THEN** the caller supplies the same semantic key/value and both execute that value

#### Scenario: Unrelated source symbol
- **WHEN** a source-symbol mapping is missing, ambiguous or semantically incompatible
- **THEN** compilation reports the key/stage/target rather than guessing from numeric IDs

### Requirement: 反射是编译产物

每次成功编译 SHALL 原子地产出请求目标的代码与完整目标反射，二者 SHALL 作为同一 artifact 缓存/发布。宏变体 SHALL 可以改变逻辑活跃集合或布局；原生 specialization 参数 SHALL 遵守既有不改变接口布局的约束，并进入相应 pipeline 选择。反射内容摘要 SHALL 用于产物完整性与计划身份，SHALL NOT 循环成为产生自身的编译输入 key。

#### Scenario: 宏变体改变活跃资源
- **WHEN** 宏变体增删一个资源
- **THEN** 各变体 artifact 分别保存自己的受检活跃集合、代码和目标映射

#### Scenario: 编译或反射失败
- **WHEN** 代码生成成功但所需反射或 metadata 验证失败
- **THEN** 编译整体失败，不发布只有代码的可消费结果

### Requirement: 统一 cache key 与 ShaderCache 接口

开发/cook 缓存 SHALL 用规范化源码依赖、entry/stage、变体与 schema/interface、TargetEnvironment、实际 ProducerIdentity 计算 CompileKey。ProducerIdentity SHALL 包含参与工具链与反射转换规则身份，生产调用 SHALL NOT 用零占位。ShaderCache SHALL 保持既有 offline/local root 职责；同一请求输入 SHALL 稳定命中，不同目标环境或生产者 SHALL 失效。Shipping SHALL 从发布 index 解析 artifact，不依赖当前安装编译器重算 CompileKey。

#### Scenario: 统一且稳定的 key
- **WHEN** 规范化输入相同，仅构建机器绝对路径不同
- **THEN** CompileKey 相同

#### Scenario: 工具链或布局模式变化
- **WHEN** 编译器、profile、平台下限、影响布局的选项或反射转换规则改变
- **THEN** CompileKey 改变，旧开发产物不能被当作新请求命中

### Requirement: Slang 编译通道

ShaderCompilerSlang SHALL 使用既有 Slang Compilation API 直接生成请求的 SPIR-V、MSL 或 DXIL 代码，并仅从该目标的 layout/metadata 生成 ABI 反射。逻辑身份 SHALL 来自同源声明身份通道，SHALL NOT 借另一个目标的 layout、短名字 first-match、资源顺序或生成文本解析恢复。虚拟 include SHALL 继续通过既有 ShaderFileSystem 及持久导出接口解析，保留源码 snapshot 与 owner/dependency 约束。

#### Scenario: SPIR-V 直接产物
- **WHEN** 请求 SPIR-V 的平坦 ConstantBuffer 程序
- **THEN** 得到非空 SPIR-V、声明逻辑键与该目标实际 ABI，不请求 MSL/DXIL layout

#### Scenario: MSL 单目标产物
- **WHEN** 请求 MSL stage
- **THEN** session 仅包含所请求的 MSL target，反射保留该目标原生类别/槽位和真实 block 布局，不创建辅助 SPIR-V layout

#### Scenario: 虚拟 include
- **WHEN** 程序 include 已注册的 owner `.slangh`
- **THEN** 经同一受检源码视图解析，无需访问作者 C++ 头或恢复已移除的嵌入 shader 链路

### Requirement: Compilation retains its source snapshot

Accepted compilation SHALL retain a stable owned source/dependency view, inventory and required provider/compiler/data ownership until completion or collected cancellation; defining DLL code SHALL remain process resident. Resource registration/retirement SHALL NOT mutate or destroy backing still used by a compile job. Failures SHALL release borrows through the same lifetime protocol.

#### Scenario: Source owner retires during compilation
- **WHEN** an owner stops accepting requests while a shader job is in flight
- **THEN** the job finishes against its retained snapshot or reports collected cancellation, and ownership is released only after that use ends

### Requirement: Shader 身份为相对路径（非资产）

ShaderRef SHALL identify an explicit source path and entry without becoming a UUID asset. Paths SHALL be canonical domain-relative paths in validated owner inventories, with normalized separators and rejected traversal. Spelling SHALL be retained; case-only path collisions SHALL fail rather than being silently lowercased. Source and relocated product resolution SHALL preserve the same identity.

#### Scenario: Portable identity
- **WHEN** a module's physical root changes but its logical shader path and entry stay the same
- **THEN** requests retain the same logical identity

#### Scenario: Case-only collision
- **WHEN** two owners expose paths differing only by case
- **THEN** collection rejects the conflict before publishing the inventory

### Requirement: 三段解析（schema / 变体 / 二进制）

Preparation SHALL obtain complete logical schema, normalize typed V/P/S values, project stage dependencies and resolve matching stage artifacts. Schema fingerprint, ownership, ranges and defaults SHALL be validated before lookup. A supplied hash SHALL NOT replace full canonical selection validation. Immutable preparation SHALL retain program, values, source/product metadata and owners across asynchronous execution.

#### Scenario: Supplied fingerprint is stale
- **WHEN** request schema or selected values disagree with their metadata
- **THEN** preparation fails instead of resolving by a caller-supplied hash alone

### Requirement: 解析优先级 离线 > 源码 > 本地实时

Resolution SHALL explicitly distinguish DEVELOPMENT and PRODUCTS. DEVELOPMENT SHALL require the current source closure and actual producer identity, validate matching offline/local cache artifacts and compile only on a valid miss. PRODUCTS SHALL resolve complete published selections from its selected product root without source or compiler access. Missing, corrupt, incompatible or ambiguous products SHALL fail explicitly; neither mode SHALL accept a blob merely because its file exists.

#### Scenario: Development cache hit
- **WHEN** source, producer, target environment and normalized selection match a validated artifact
- **THEN** development loads that artifact without recompilation

#### Scenario: Products without compiler
- **WHEN** a relocated product distribution contains the requested complete selection and compatible runtime ABI
- **THEN** resolution succeeds without sources or the producer toolchain

#### Scenario: Unpublished selection
- **WHEN** PRODUCTS receives a selection absent from its manifest/index
- **THEN** it reports missing product without fallback compilation or substituting defaults

### Requirement: 内容寻址缓存（单 index + blobs）

Each target directory SHALL contain a versioned index and content-addressed stage artifacts. The current cache format SHALL preserve complete logical schema, allowed selections, code identities, target environment and producer metadata; code plus reflection SHALL be checked as one ArtifactID. Offline and local roots SHALL retain explicit read/write roles. Packaging SHALL combine selected owners' records with conflict validation instead of first-match mounting that hides another index. Incompatible formats SHALL require recook.

#### Scenario: Multiple shader owners
- **WHEN** two selected owners contribute distinct requests for the same target
- **THEN** the distributed index retains both; conflicting request identities fail

#### Scenario: Artifact corruption
- **WHEN** code or reflection differs from the recorded ArtifactID
- **THEN** loading rejects the artifact before native creation

### Requirement: 编译键与 entry 级产物

Compile identity SHALL include normalized source/dependency closure, entry/stage, code-affecting selection, schema/interface, target environment and actual producer identity. Complete logical selection identity SHALL remain distinct from stage code and native executable identity. Compatible native-specialization selections MAY share code, but SHALL retain independent stage-specific values and executable selection. Artifact identity SHALL bind exact code length and target reflection; reflection output SHALL NOT become a circular compile input.

#### Scenario: Native selections share code
- **WHEN** two complete selections differ only in a supported native specialization value
- **THEN** code may be reused while executable identity and stage values remain distinct

#### Scenario: Dependency changes
- **WHEN** an included export, companion, target option or compiler adapter changes
- **THEN** affected compile identities invalidate old development results

### Requirement: 变体 key 组装

Production selection SHALL use canonical typed V/P/S keys. Mesh capabilities supply Vertex values; pipeline supplies Pipeline values; Material/custom Batch/fullscreen/compute supply Shader-local values. Owner, type, range, duplicate and cross-stage consistency SHALL be validated; explicit and omitted defaults SHALL normalize identically. Full selection SHALL NOT be limited by the retained 128-bit compatibility helper, and business values SHALL NOT choose macro/native IDs or policy.

#### Scenario: Cross-domain override
- **WHEN** Material tries to set a Vertex skinning key or Pipeline shadow key
- **THEN** selection fails without changing the previous prepared material

#### Scenario: Fullscreen without mesh
- **WHEN** fullscreen supplies P/S inputs and no Vertex inputs
- **THEN** it uses the same normalization contract and requires no dummy Mesh or Material

### Requirement: schema 载体为 companion `.slang.json`

per-shader schema SHALL 由与源码同目录的 companion `.slang.json` 声明，且 SHALL 计入 `sourceHash`。schema 解析 SHALL 遵循 **源码 companion > 离线 index**。

#### Scenario: companion 优先

- **WHEN** 源码旁存在 companion 且离线 index 也有该 schema
- **THEN** 使用 companion 的 schema 与 schemaFp

#### Scenario: 无 companion 用离线

- **WHEN** 无源码 / 无 companion，离线 index 有 schema
- **THEN** 使用离线 schema

### Requirement: 单写者 IO 与原子落盘

Cache publication SHALL serialize index changes and atomically replace temporary files only after successful encoding and write. ShaderCacheWriter SHALL provide queued single-writer operation; synchronous commits SHALL use the existing guarded store. An unsupported or failed rename SHALL return failure rather than fall back to exposing a partial direct write. Async jobs SHALL retain owned compiler and cache endpoints until completion; errors SHALL remain observable through commit/flush results.

#### Scenario: Concurrent submissions
- **WHEN** several jobs publish different artifacts through the writer
- **THEN** successful flush retains all committed records

#### Scenario: Rename fails
- **WHEN** final atomic replacement fails
- **THEN** publication reports failure and the prior published index remains readable

### Requirement: 编译器抽象与可选模块

IShaderCompiler and its shared factory SHALL keep the compiler optional for PRODUCTS consumption. DEVELOPMENT SHALL require the actual producer identity and valid sources, compiling only a cache miss. Runtime-only compiler registration SHALL be revoked on shutdown; accepted asynchronous work SHALL retain its compiler and cache endpoint owners until completion; defining DLL code SHALL remain process resident. Missing compiler ownership for an async miss SHALL fail explicitly.

#### Scenario: Products without compiler
- **WHEN** no compiler module is loaded and PRODUCTS has a compatible published selection
- **THEN** the artifact loads without reconstructing a compiler identity

#### Scenario: Async task survives unregister
- **WHEN** a compiler is unregistered after a task was accepted
- **THEN** that task retains its owned compiler and cache endpoints until completion without a per-object code token

### Requirement: 编译任务可内联或异步调度

Resolver SHALL 把编译拆成自包含的 `ShaderCompileTask`，由调用方选择执行线程：内联（当帧 render parallel，阻塞、不可跳过）或异步（worker，`Ready()` 前可跳过）。编译产物 SHALL 经 `CommitCompile` 落缓存（writer 队列或同步）。

#### Scenario: 异步编译

- **WHEN** `ResolveShaderAsync` 返回 `Compile` 且调用方 `RunAsync(pool)`
- **THEN** `Ready()` 后 `Succeeded()` 为真，且结果已提交缓存

#### Scenario: 内联编译

- **WHEN** 得到 `Compile` 后调用 `RunInline()`
- **THEN** 立即完成并提交缓存

### Requirement: 运行时 Shader 创建

解析得到的 blob + reflection SHALL 经 `Device::CreateShaderFunction` / `CreateShader` 创建 RHI `Shader`；该创建 SHALL 在 render 线程进行（Device 独占）。

#### Scenario: 从 blob 建 Shader

- **WHEN** 解析出 blob 与 reflection
- **THEN** 在 render 线程创建 `ShaderFunction`（vs/ps）与 `Shader`

### Requirement: Shader 使用收集（离线预编）

Usage collection SHALL be explicitly enabled by the host and record source/entry/stage/target environment plus complete typed selection. Persistent identity SHALL NOT rely only on a 128-bit compatibility key or hash. Flush SHALL union compatible records across sessions with checked format/version handling. Offline cook SHALL consume explicit requested selections, project each stage's dependencies and publish the allowed complete selections; it SHALL NOT infer an unrestricted Cartesian product or replace nondefault values with defaults.

#### Scenario: Complete selection survives collection
- **WHEN** V/P/S inputs include nondefault bool, signed and unsigned values
- **THEN** usage/cook retain all typed values and their stage-specific meaning

#### Scenario: Collection disabled
- **WHEN** the host did not enable usage collection
- **THEN** resolution does not implicitly publish a usage file

#### Scenario: Session union
- **WHEN** two enabled sessions collect distinct valid selections
- **THEN** the published usage union retains both without duplicate identities

### Requirement: slang DXIL 复用引擎 dxcompiler 包

`SLANG_ENABLE_DXIL=ON` 时，slang 构建 SHALL 从 `${3RD_PATH}/dxcompiler/bin` 拷贝 `dxcompiler.dll` 与 `dxil.dll` 到 slang 构建输出 bin（`copy-dxcompiler` / `copy-dxil` target），SHALL NOT `include(FetchDXC)` 从网络下载 DXC。

slang 构建 SHALL 用 `${3RD_PATH}`（`third_party.py` 注入的 CMake 变量）定位 `dxcompiler` 包，SHALL NOT 写死本地全路径（遵守 coding rules Rule 7）。

#### Scenario: 不触发网络下载

- **WHEN** 全新 bootstrap（`build_3rd/intermediate/slang` 无 `_deps/dxc-*` 缓存）构建 slang
- **THEN** configure 阶段不出现 `Downloading DXC from https://...`，不执行 FetchContent 下载

#### Scenario: 复用引擎 DXC

- **WHEN** slang 构建的 DXIL 编译路径解析 `dxcompiler.dll` / `dxil.dll`
- **THEN** 来源为 `${3RD_PATH}/dxcompiler/bin`（引擎 `dxcompiler` 包，版本 `v1.9.2602`）

#### Scenario: 无本地全路径

- **WHEN** 阅读 `cmake/patches/slang.patch` 中 DXIL 相关 hunk
- **THEN** 不含盘符/主目录前缀的绝对路径，仅用 `${3RD_PATH}` / `${CMAKE_BINARY_DIR}` 等 CMake 变量

#### Scenario: DXIL 编译仍可用

- **WHEN** 重建后运行 `AuroraShaderTest` 的 `CompileDxilForDx12` 与 `AuroraD3D12Test.*`
- **THEN** 全部通过，DXIL 产物与反射结果不变

#### Scenario: aurora shader 模块声明 dxcompiler 运行时依赖

- **WHEN** Windows 上构建 `Aurora.Shader` / `AuroraShaderTest`
- **THEN** `3rdParty::dxcompiler` 的 `dxcompiler.dll` + `dxil.dll` 被自动拷贝到运行时输出目录（`output/bin/Debug`），无需手动拷贝；DXIL 编译在干净环境也能找到这两个 DLL

### Requirement: slang patch 兼容 Xcode generator

slang patch（`cmake/patches/slang.patch`）SHALL 保证 slang 在 Xcode generator 下 configure/generate 成功。patch 引入或保留的 per-source `COMPILE_OPTIONS` SHALL NOT 包含 `$<CONFIG:...>` 等 per-config generator 表达式；需要按配置区分行为时 SHALL 在 configure 期按编译器分支（如 `if(MSVC)`），仅在 Visual Studio generator 使用的分支内使用 per-config genex。

#### Scenario: Xcode configure 成功

- **WHEN** `python third_party.py -p MacOS-x86 -t slang -f` 构建 slang
- **THEN** configure/generate 阶段不出现 `Xcode does not support per-config per-source COMPILE_OPTIONS`，Debug + Release 编译安装成功

#### Scenario: MSVC 语义不变

- **WHEN** Win32（Visual Studio generator）构建 slang
- **THEN** `slang-rich-diagnostics.cpp` 在 Debug 下跳过 size 优化，非 Debug 使用 `/O1;/Os`，与上游行为一致

#### Scenario: GCC/Clang 全配置 size 优化

- **WHEN** Xcode / Ninja 等 generator 下用 Clang 或 GCC 构建 slang
- **THEN** `slang-rich-diagnostics.cpp` 在所有配置下分别使用 `-Oz`（Clang）/ `-Os`（GCC）

### Requirement: Pass 绑定按实际 shader 子集装配

绑定装配 SHALL 按实际 shader reflection 校验所提供共享 schema 的资源与 uniform 布局。未被 shader 使用的声明可省略；实际使用但缺少声明、缺少绑定值、重复提供或类型/形状冲突 SHALL 在 GPU 执行前失败。装配失败 SHALL 不改变原 draw；成功 SHALL 输出现有图绑定并沿用子资源分析与退休协议。

#### Scenario: 可选资源
- **WHEN** shader variant 不使用 Pass 声明中的纹理
- **THEN** 装配不要求该纹理的运行时绑定

#### Scenario: 上游 Pass 输出作为输入
- **WHEN** 下游 Pass 将上游写入的图 image 绑定到共享声明的采样槽位
- **THEN** 读写依赖作用于同一图资源及实际 view 范围，提交后的资源保持有效至退休

### Requirement: Renderer 保持四层资源语义

Renderer SHALL 使用 Global set 0、Pass set 1、Material set 2、Batch set 3 的资源分层。具体 Pass SHALL 拥有 Pass 层声明，View SHALL 为按需输入；系统 SHALL NOT 将四层重新规定为固定 View/Material/Object 块，或强制所有 shader 声明相机常量。材质数据 SHALL 归 Material，逐 draw 数据 SHALL 归 Batch；动态偏移 SHALL 服从实际声明和 shader reflection，不由 set 编号猜测字段、大小或类型。

#### Scenario: 无 View 的 Pass
- **WHEN** 一个 Pass 的 shader 与资源声明均不包含 View 数据
- **THEN** renderer 接纳匹配声明，不要求补充相机 UBO 或固定 80 字节块

#### Scenario: 不同 Pass 参数布局
- **WHEN** 两个 Pass 在各自 set 1 使用不同参数类型与大小
- **THEN** 两者分别按各自共享布局验证和绑定，不能用 Material 固定布局覆盖任一 Pass

### Requirement: 布局共享与帧数据隔离

系统 SHALL 允许共享不可变 shader/layout/PSO；可变 Global/Pass/Material/Batch 数据 SHALL 保持当前 frame lease 和使用范围内的隔离，数据与引用 SHALL 保活至对应 GPU 使用退休。动态绑定 SHALL 按声明/反射校验范围、对齐和顺序，不得从资源层名称推导 dynamic 类型。

#### Scenario: 多 View 与在途帧
- **WHEN** 同一 Pass 定义用于两个 View，且上一帧尚在 GPU 执行
- **THEN** 每次使用读取其对应的常量，不会被后续 View 或下一帧覆盖

### Requirement: 完整 Pass 资源声明

具体 Pass SHALL 从同一共享头展开 uniform 类型、类型化槽位与期望 schema，保留 set/binding、类型、维度和 sampler 形状。资源-only 声明 SHALL 可用；不支持的声明 SHALL 显式失败，不得静默遗漏。C++ 不再依赖反射生成镜像，构建与类型子集由 refactor-aurora-shared-pass-headers 规定。

#### Scenario: 常量与纹理同源
- **WHEN** Pass 声明常量、纹理和 sampler
- **THEN** C++ 与 shader 消费同一源，期望 schema 与实际目标反射一致

#### Scenario: 仅有资源
- **WHEN** Pass 不含常量块
- **THEN** 仍暴露资源表与类型化槽位，不强制占位常量

### Requirement: Owner-scoped shared declarations

Shared declarations SHALL use one authoritative C++ `.h` source with owner-selectable namespaces. C++ consumers SHALL include that source without requiring Slang compilation or shader-header export. CMake tooling SHALL provide deterministic export/update and read-only verification of shader-only `.slangh` assets. Exports SHALL persist in the module's source-tree `assets/shaders/` root, be tracked in version control, and preserve the declaration's domain-relative path with only the extension changed. Generated headers SHALL NOT be independently editable sources or complete shader programs. Global/Pass/Material/Batch SHALL use sets 0/1/2/3, with optional View data inside Pass.

#### Scenario: Two plugins reuse local names
- **WHEN** independently built plugins declare ToonPass and ToonParams in separate namespaces
- **THEN** their C++ declarations coexist without symbol collisions and their respective shaders consume the corresponding exported interfaces

#### Scenario: C++ build without shader compiler
- **WHEN** a C++ target consumes an owner's shared declaration with Slang unavailable
- **THEN** its types, slots and expected layout metadata compile directly from the authoritative header without compiling shaders or executing a target-platform generator

#### Scenario: Deterministic shader header export
- **WHEN** the same declaration, helper and exporter inputs are exported repeatedly
- **THEN** they produce identical persisted headers without changing files whose contents are already current, and updated inputs require corresponding export and shader-cache identity updates

#### Scenario: Fresh checkout or build clean
- **WHEN** shader assets are read from a fresh checkout or CMake build outputs are cleaned
- **THEN** the tracked `.slangh` assets remain available under the module's assets/shaders root without relying on a generated build-directory copy

#### Scenario: Stale or manually modified export
- **WHEN** source-checkout shader build/cook or CI finds a missing export or one inconsistent with its registered declaration/helper/exporter inputs
- **THEN** verification reports the source/output mismatch without silently modifying tracked assets or shipping inconsistent interfaces

#### Scenario: Configuration-independent export
- **WHEN** different build configurations or shader backends consume the same shared declaration
- **THEN** they consume the same persisted interface without racing to overwrite it with configuration-specific output

#### Scenario: Export collides with handwritten shader helper
- **WHEN** an export mapping claims a path already owned by a handwritten `.slangh` file
- **THEN** registration/export fails before overwriting the existing asset

#### Scenario: Unsupported declaration syntax
- **WHEN** a shared header uses a construct outside the supported export grammar
- **THEN** export fails explicitly with source diagnostics rather than emitting an incomplete or guessed shader layout

#### Scenario: Shader consumes the exported interface
- **WHEN** a material shader targets OpaquePass
- **THEN** it can include the exported Global and OpaquePass shader headers without including C++ headers or acquiring a generated complete material program

### Requirement: Logical bindings remain independent of native argument slots

Compiled reflection and shipped products SHALL retain authored logical set/binding identities separately from native resource indices for each shader stage. Uniform sizes and member offsets SHALL come from the selected target's layout. Backends SHALL bind the recorded native indices rather than infer them from merged resource order. Unsupported or inconsistent mappings SHALL fail explicitly.

#### Scenario: Metal flattens register spaces
- **WHEN** two active uniform buffers use binding zero in different logical sets
- **THEN** they retain both logical identities and receive distinct Metal buffer arguments

#### Scenario: Stages use different native indices
- **WHEN** a logical resource has different native indices in vertex and fragment stages
- **THEN** the backend binds that resource at each stage's recorded index, including its draw-specific dynamic offset

#### Scenario: Storage image has mixed reflection categories
- **WHEN** an active storage image has multiple native reflection categories
- **THEN** its logical binding and actual native texture index survive compilation, cook and source-free runtime lookup

### Requirement: Unified module resource registration

Plugin/module registration SHALL declare shader import rules once in a resource description available to project assembly and offline tools without loading rendering runtime DLLs. Common owner, source-root, namespace, inventory, dependency and distribution metadata SHALL be shared with asset-resource registration where applicable. Shader include/header exports and path identities SHALL retain their own semantics; sharing registration SHALL NOT require converting ShaderRef into a UUID asset. Aurora foundation and project modules SHALL use the same contribution contract without becoming business plugins.

#### Scenario: One declaration feeds all consumers
- **WHEN** an enabled plugin contributes shader resources and asset resources through its module metadata
- **THEN** project collection supplies the relevant rules to development loading, compilation, cook and packaging without separate manually synchronized shader lists

#### Scenario: Runtime DLL is unavailable during cook
- **WHEN** project cook collects declared plugin shader inputs on a host without that plugin's rendering runtime DLL
- **THEN** it obtains resource descriptions and offline-accessible source providers without instantiating runtime rendering services

#### Scenario: Custom filesystem without enumeration
- **WHEN** a module supplies a custom IFileSystem provider
- **THEN** registration requires an explicit immutable file inventory and retained provider ownership, and cook uses an equivalent offline-accessible provider or staged input

#### Scenario: Project selects module contributions
- **WHEN** a plugin is compiled but not selected by the project's resource graph
- **THEN** its roots are not implicitly mounted merely because the plugin exists on disk

### Requirement: Uniform module shader roots and unique paths

Shader-bearing module owners SHALL collect `.slang`, `.slangh` and companion assets under their `assets/shaders/` roots. Aurora SHALL use `engine/aurora/assets/shaders/` across its internal targets; each plugin SHALL use its own `plugins/<plugin-id>/assets/shaders/` root. ShaderRef, includes/imports, dependency hashing and packaging SHALL use canonical domain-relative paths inside those roots, independently of physical source/package locations. Exported headers SHALL reside alongside handwritten assets and preserve the source declaration's domain-relative path and spelling. Existing MultiFileSystem SHALL compose configured providers only after registration validates complete inventories, allowed dependency closure and unambiguous include mappings. Conflicts SHALL fail atomically; mount/include order SHALL NOT resolve them.

#### Scenario: Aurora shared declaration export
- **WHEN** the registered shared declaration has logical include path `aurora/pipeline/OpaquePass.h`
- **THEN** its export resides at `engine/aurora/assets/shaders/aurora/pipeline/OpaquePass.slangh` and shader includes use `aurora/pipeline/OpaquePass.slangh`

#### Scenario: Independent plugin asset roots
- **WHEN** plugin toon exports `toon/pipeline/ToonPass.h`
- **THEN** the persisted file is `plugins/toon/assets/shaders/toon/pipeline/ToonPass.slangh`, its shader include is `toon/pipeline/ToonPass.slangh`, and it needs no repository-wide plugins/assets/shaders root

#### Scenario: Same basename in separate owners
- **WHEN** two plugins export local ToonPass.slangh headers under different owner prefixes
- **THEN** qualified includes resolve to the intended header regardless of registration order or physical package location

#### Scenario: Conflicting registration is atomic
- **WHEN** a contribution conflicts with an existing owner, canonical file path or generated output
- **THEN** registration fails with both providers identified and previously published resources remain unchanged

#### Scenario: Case-only collision
- **WHEN** two contributions expose paths differing only in case
- **THEN** collection rejects them before publication without silently changing existing ShaderRef spelling

#### Scenario: Include dependency is undeclared
- **WHEN** an include requests a resource outside the owner and its declared resource dependencies or escapes its allowed root
- **THEN** resolution fails explicitly rather than searching another owner's files

#### Scenario: Custom source and include roots
- **WHEN** a module configures a non-default physical root or provider with the prescribed namespace
- **THEN** compilation, dependency collection and cook resolve the same canonical inputs without changing ShaderRef identity

#### Scenario: Reversed mount order
- **WHEN** the same validated non-conflicting provider set is composed in another order
- **THEN** every shader/include resolves to the same provider and content

#### Scenario: Local include resolution
- **WHEN** a shader uses a quoted local include
- **THEN** its logical directory and permitted include roots resolve one canonical candidate, or report ambiguity, without cross-owner basename fallback

#### Scenario: Macro includes and module imports
- **WHEN** a shader uses macro-expanded includes or Slang module imports
- **THEN** dependency identity covers those inputs through compiler-observed dependencies or a conservative complete declared closure, and changing their contents invalidates dependent results

### Requirement: Runtime and cook share owner resource descriptions

Development compilation and offline cook SHALL consume the same collected resource descriptions, logical identities and dependency contents. The existing project cook/package entry SHALL collect shader requirements from material techniques and explicit FullScreenPass/ComputePass techniques, include compiled products, reflection, schema and required variants in selected distributions, and provide runtime lookup without per-module manual copy/cook steps. Product lookup SHALL retain all selected owners' shader records rather than rely on first-match per-module cache indices.

#### Scenario: Staged sources match development inputs
- **WHEN** an owner/dependency graph is compiled from native development files and equivalent staged cook inputs
- **THEN** both resolve the same logical sources, schema and dependency contents and produce compatible target reflection

#### Scenario: Source distribution contains exported interfaces
- **WHEN** a module's shader sources are staged for a source or SDK distribution
- **THEN** its persisted `.slangh` files are collected with `.slang` and companion assets from the same registered root, preserving include paths without requiring the consumer to regenerate headers

#### Scenario: Shared dependency edit
- **WHEN** an exported header or declared dependency changes
- **THEN** affected build outputs and runtime/cook cache identities refresh without conflating another owner's same-basename resource

#### Scenario: Unified project packaging
- **WHEN** a project packages selected Aurora, plugin and project contributions through normal cook/package entry points
- **THEN** shader requests from scene materials and explicit fullscreen/compute techniques enter the selected distribution without separate shader deployment steps

#### Scenario: Several owners provide cache records
- **WHEN** two selected plugins contribute shader records for the same backend
- **THEN** the distributed index contains both owners' requests and blobs, and conflicts fail explicitly rather than one plugin's index hiding the other

#### Scenario: Relocated package without shader sources
- **WHEN** a packaged project runs after relocation with development source roots unavailable and runtime Slang absent
- **THEN** required scene, fullscreen and compute requests resolve from shipped products without embedded-source compilation fallback

#### Scenario: Runtime selects existing product contents
- **WHEN** a filesystem can create a valid child wrapper for an absent product directory
- **THEN** the host SHALL validate the shader distribution contents before selecting products
- **AND** development mode SHALL register declared source resources, while product mode SHALL report absent or malformed selected products explicitly

#### Scenario: Native build and package deploy the same selected resources
- **WHEN** a project is built through Android Studio, Apple IDE generation or a single-configuration desktop generator
- **THEN** its selected shader resources SHALL be staged into the actual runtime resource root using the same collected graph as packaging
- **AND** staging SHALL NOT derive an absolute system-root destination from an unset runtime output directory
