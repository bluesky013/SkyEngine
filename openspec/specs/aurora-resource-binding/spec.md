# aurora-resource-binding Specification

## Purpose
Define logical resource groups, checked parameter transactions, target-specific binding and uniform encoding, and the lifetime of published resource snapshots.
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
