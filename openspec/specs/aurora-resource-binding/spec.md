# aurora-resource-binding Specification

## Purpose
TBD - consolidated.
## Requirements

### Requirement: 跨后端 binding 索引一致性约定

Aurora SHALL 保证：在 layout 中声明的 (set, binding) 索引在所有后端表现一致：

- Vulkan：直接 `layout(set=N, binding=M)`
- DX12：通过 root signature 的 register space + register 映射（space=N, register=M）
- Metal：argument buffer slot=N，buffer/texture index 由 layout 表内部分配

调用方写一份 layout，多后端 binding 行为一致；shader 编译产物可不同（每后端单独编译），但 layout 描述统一。

#### Scenario: 同一 layout 跨后端

- **WHEN** 用同一 layout（set 0：binding 0=UB, binding 1=COMBINED）+ 各自后端编译的 shader 创建 pipeline
- **THEN** 各后端中 BindResourceGroup + Draw 行为一致（采到同一纹理、读到同一 UB 数据）

### Requirement: Device::CreateResourceGroup 创建 group 实例

`Device::CreateResourceGroup(const ResourceGroup::Descriptor &)` SHALL 按 `Descriptor::{shader, set}` 分配一个新的 ResourceGroup 实例。

`shader` MUST 非空；`set` SHALL 是 shader reflection 中存在的 descriptor set。返回的 ResourceGroup 在创建后所有 binding 均为"未写入"状态，必须先 `Update` 才能在 BindResourceGroup 后被采样。ResourceGroup 的 binding 布局 SHALL 等于 shader reflection 中该 set 的完整资源集。

#### Scenario: 创建 group

- **WHEN** 调用 `device->CreateResourceGroup({.shader = shader, .set = 0})`
- **THEN** 返回非空 `ResourceGroup*`

#### Scenario: shader 为空拒绝

- **WHEN** `Descriptor::shader = nullptr`
- **THEN** 返回 nullptr

### Requirement: ResourceGroup::Update 批量写入资源

`ResourceGroup` SHALL 通过 `CreateEncoder()` 返回的 `DescriptorEncoder` 写入资源：调用 `WriteBuffer` / `WriteImage` / `WriteSampler` 按 (binding, arrayElement) 积累写入，`End()` 一次性提交到 descriptor set。

写入的 (binding, kind) 组合 MUST 与 shader reflection 中该 binding 的 `ShaderResourceType` 兼容；不兼容时 debug build assert，release build 行为未定义。

`End()` 可被多次调用（每次一组新写入）；新写入覆盖旧写入；未被覆盖的 binding 保留先前值。

#### Scenario: 写入 uniform + sampled image

- **WHEN** 对 set 含 binding 0=uniform buffer、binding 1=sampled image、binding 2=sampler 的 group，用 encoder `WriteBuffer(0, ubuf, 0, 64)` + `WriteImage(1, tex, SHADER_READ_ONLY)` + `WriteSampler(2, smp)` 后 `End()`
- **THEN** `End()` 不报错；后续 BindResourceGroup + Draw 可正确采样到 tex 与 ubuf 数据

#### Scenario: 写入类型不匹配 binding type

- **WHEN** 对 binding=0（uniform buffer）调用 `WriteImage`
- **THEN** Debug build assert；Release build 行为未定义但 MUST 不静默成功

### Requirement: Encoder::BindResourceGroup 真正生效，支持动态偏移

`GraphicsEncoder::BindResourceGroup(uint32_t set, ResourceGroup *group, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets)` 与 `ComputeEncoder::BindResourceGroup(...)` SHALL 把 group 绑定到当前 pipeline 的对应 set 槽。

`set` SHALL 对应当前 pipeline 经 shader 反射派生出的某个 descriptor set。

`BindResourceGroup` SHALL 绑定该 group 的**当前帧 set 视图**：transient 模式（Global/Pass）下是本次更新刚申请的 set；cached 模式（Batch）下是持久 set。绑定前 SHALL 完成该 group 的写入提交（Vulkan `vkUpdateDescriptorSetWithTemplate` / `vkUpdateDescriptorSets`；DX12 的 staging→shader-visible copy）。

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

ResourceGroupLayout 中 `count > 1` 的 binding（如纹理数组）MUST 支持通过 `Update` 按 `arrayElement` 索引写入；BindResourceGroup 后 shader 可以索引 `arr[i]`。

#### Scenario: 8 元素 sampled image 数组

- **WHEN** layout binding=0、type=COMBINED_IMAGE_SAMPLER、count=8；Update 8 次（arrayElement 0..7）；Draw shader 读 arr[3]
- **THEN** 采样到 arrayElement=3 写入的纹理


### Requirement: DescriptorEncoder 接口

`DescriptorEncoder` SHALL 是接口层抽象类，提供 `WriteBuffer` / `WriteImage` / `WriteSampler` 三个写入方法与 `End()` 提交方法：

- `WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0)`
- `WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0)`
- `WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0)`
- `End()` 把积累的写入 flush 到目标 descriptor set

`DescriptorEncoder` SHALL 不暴露 `ResourceUpdateInfo` 或任何通用中间结构；写入由后端直接积累为 native 结构。

#### Scenario: 链式批量写入

- **WHEN** 依次调用 `WriteBuffer` / `WriteImage` / `WriteSampler` 后调用 `End()`
- **THEN** 所有写入一次性提交到 descriptor set，后续 `BindResourceGroup` + Draw 可采样到对应资源

### Requirement: ResourceGroup::CreateEncoder 返回后端 encoder

`ResourceGroup` SHALL 提供 `CreateEncoder()`，返回绑定到该 group 的 descriptor set 的后端 `DescriptorEncoder*`。encoder SHALL 只作用于创建它的那个 group（Vulkan 目标 `VkDescriptorSet`；DX12 目标 root 参数）。

#### Scenario: 从 group 创建 encoder

- **WHEN** 调用 `group->CreateEncoder()`
- **THEN** 返回非空 `DescriptorEncoder*`，其 `End()` 写入该 group 的 descriptor set

### Requirement: 后端原生实现

Vulkan 后端 SHALL 提供 `VulkanDescriptorEncoder`：`Write*` 写入 RG 的持久化 packed buffer 并置 dirty，`End()` SHALL 在 dirty 时调用 `vkUpdateDescriptorSetWithTemplate`。`VulkanShader` SHALL 为每个 set 建 `VkDescriptorUpdateTemplate`（core 1.1，Aurora 要求 1.3 故始终可用）；template 创建失败时 `End()` 回退 `vkUpdateDescriptorSets`。

DX12 后端 SHALL 提供 `D3D12DescriptorEncoder`：按 reflection 类型分发——静态绑定写 descriptor table 的 CPU handle，动态绑定（`UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC`）记录 buffer + baseOffset + range（bind 时由 root CBV/UAV 带 offset 重绑）。

Metal 后端 SHALL 提供 `MetalDescriptorEncoder`（接口就位，argument buffer 写入随 `aurora-resource-group Metal phase` 落地，暂为空实现 + TODO）。

#### Scenario: Vulkan 批量提交

- **WHEN** `VulkanDescriptorEncoder` 多个 `Write*` 后 `End()`
- **THEN** dirty 时调用一次 `vkUpdateDescriptorSetWithTemplate` 写入全部 binding；非 dirty 时跳过（无 flush）

#### Scenario: DX12 动态绑定分发

- **WHEN** 对 `UNIFORM_BUFFER_DYNAMIC` binding 调用 `WriteBuffer`
- **THEN** `D3D12DescriptorEncoder` 记录 buffer + baseOffset + range，不写 descriptor（bind 时走 root CBV）

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

`RgBlockDesc` SHALL 描述一个 resource block：`{ set, binding, blockName, fields[] }`。其 `set/binding/kind/fields` SHALL 由 `.slang` shader 反射生成（而非手写），同一份 desc SHALL 可产出：

- RHI `ResourceGroupLayout::Descriptor`（cbuffer → `UNIFORM_BUFFER`/`UNIFORM_BUFFER_DYNAMIC`；texture/sampler → 对应 `DescriptorType`）
- 供 shader `#include` 的 Slang 共享头（struct + `ParameterBlock` + `[[vk::binding]]`）
- C++ 镜像 struct（与 shader 布局一致，带 `static_assert` 校验）

C++ 侧 UBO 写入与 shader 声明 SHALL 使用同一 offset 表（offset 来自 slang 反射）。

#### Scenario: 反射派生 desc

- **WHEN** 对含 `[[vk::binding(0, 0)]] ParameterBlock<GlobalParams> gGlobal` 的 `.slang` 做 codegen
- **THEN** 生成 `RgBlockDesc{set=0, binding=0, blockName="Global", kind=CBUFFER}`，字段与 `GlobalParams` 成员一致

#### Scenario: 一致性

- **WHEN** 同一 `.slang` 反射分别产出 layout、shader 头、C++ struct
- **THEN** binding 编号、字段顺序、类型、offset 在两侧一致

### Requirement: Global tier 数据流

`GlobalRenderResources`（aurora/pipeline）SHALL 持有 global UBO 与 global RG，提供 `UpdateView(const SceneView&, float time)`（每帧写 UBO + RG.Update）。

`RenderGraph::SetGlobalResourceGroup(ResourceGroup*)` SHALL 把 global RG 写入 `CompiledGraph::globalResourceGroup`；executor SHALL 在每个 raster/compute pass 开始前 `BindResourceGroup(0, globalRG)`。

#### Scenario: 每帧更新
- **WHEN** `UpdateView(view, time)` 后 `Compile` + `Execute`
- **THEN** executor 在每 pass 前绑定 global RG 到 set 0

### Requirement: Pass tier 生命周期

`PipelinePass` SHALL 支持声明 pass 级 `RgBlockDesc`；`OnSetup` 由其创建 layout + RG（持久），`OnSceneChanged` 重建；`BuildRDG` 经 builder 传入（现有 SetPassResourceGroup/SetQueueResourceGroup wiring 不变）。

#### Scenario: pass RG 持久复用
- **WHEN** pass 连续多帧 BuildRDG 且未 OnSceneChanged
- **THEN** pass RG 复用同一实例

### Requirement: Batch tier dynamic UBO

`BatchAllocator` SHALL 管理 batch tier（set 2）的 dynamic UBO：纯线性分配器，`Allocate(size) -> offset` 按 `DeviceCapability::minUniformBufferOffsetAlignment` 对齐分配，`Write(offset, data, size)` 写入，`Reset()` 清 cursor；不感知帧，in-flight 安全由 `DeviceFrameContext` 维护 per-inflight-frame 的 pack buffer。

batch RG 的 dynamic UBO descriptor SHALL 以 `offset=0, range=blockSize` 绑定（每帧一次，帧内不变）。`DrawItem` SHALL 含 `batchDynamicOffset`（其值为 allocator 产出的 pack offset）；executor 绑 set 2 SHALL 传 dynamicOffsets。

#### Scenario: 两个 draw 共用 RG 不同 offset

- **WHEN** 两个 DrawItem 同 batchRG、不同 `batchDynamicOffset`
- **THEN** executor 两次 `BindResourceGroup(2, rg, 1, &offset)` 分别使用各自 offset

#### Scenario: per-inflight-frame 不覆盖

- **WHEN** FrameContext 维护多个 pack buffer，连续帧各自分配写入并提交
- **THEN** 当前帧写入的 buffer 与仍在 GPU 读取的帧 buffer 不同，不覆盖


### Requirement: dynamic UBO stable binding 契约

Batch tier 的 dynamic UBO descriptor SHALL 以 `offset=0, range=blockSize` 绑定（`blockSize` 为该 RG 对应 shader block 的固定大小，std140 后 16B 对齐），每帧绑定一次（非 per-object）；帧内每 draw 仅通过 dynamic offset 选择 pack buffer 内的数据窗口。

`ResourceUpdateInfo::bufferRange` SHALL 在 DYNAMIC 类型绑定中为 `blockSize`（MUST 非 0）；Vulkan 后端对 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC` + `range==0` MUST 在 debug 断言 / 打 warning（不得静默退化为 `VK_WHOLE_SIZE`）。

#### Scenario: 帧内只写一次 descriptor

- **WHEN** batch RG 对本帧 pack buffer `Update({binding=0, BUFFER, buffer=packBuffer, offset=0, range=blockSize})` 一次，帧内多次 draw 用不同 dynamic offset
- **THEN** 帧内 descriptor set 无需再次 `Update`；每次 draw 只改 dynamic offset 即可正确采样到各自数据窗口

#### Scenario: DYNAMIC 类型 range 为 0 拒绝

- **WHEN** 对 `UNIFORM_BUFFER_DYNAMIC` binding 写入 `bufferRange=0`
- **THEN** Vulkan 后端 debug 断言 / 打 warning；release 不静默用 `VK_WHOLE_SIZE`

### Requirement: BatchAllocator 线性分配器，in-flight 由 FrameContext 维护

`BatchAllocator` SHALL 是纯线性分配器：`Init(device, capacity)` 创建单 host-visible buffer，`Allocate(size) -> offset` 在当前 buffer 内按 `DeviceCapability::minUniformBufferOffsetAlignment` 对齐分配，耗尽返回 `UINT32_MAX`；`Write(offset, data, size)` 写入映射区；`Reset()` 清 cursor。

`BatchAllocator` SHALL 不感知帧：无 `inflightNum`、无 `BeginFrame(frameIndex)`。in-flight 安全 SHALL 由 `DeviceFrameContext` 维护 `inflightNum` 个 pack buffer（每 in-flight frame 一个），帧完成后回收并 `Reset()` 复用。

#### Scenario: 线性分配

- **WHEN** 对单 buffer 连续 `Allocate(64)` 两次
- **THEN** 第二次 offset = 首次 offset + 对齐后的 64（偏移按 `minUniformBufferOffsetAlignment` 对齐）

#### Scenario: 分配耗尽

- **WHEN** buffer 剩余空间小于请求 size
- **THEN** `Allocate` 返回 `UINT32_MAX`

#### Scenario: Reset 复用

- **WHEN** `Reset()` 后再次 `Allocate`
- **THEN** 从 buffer 起始重新分配

#### Scenario: in-flight 由 FrameContext 维护

- **WHEN** FrameContext 持有 `inflightNum` 个 allocator，帧完成回收后 `Reset` 复用
- **THEN** 复用不覆盖仍在 GPU 读取的帧数据（回收由 frame fence 驱动）

### Requirement: offset 对齐取自 device capability

`DeviceCapability` SHALL 含 `minUniformBufferOffsetAlignment`（默认 256）。Vulkan / DX12 / Metal 后端 SHALL 在 `UpdateDeviceCaps()` 中从原生 limits 填真实值（Vulkan `minUniformBufferOffsetAlignment`、DX12 `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`、Metal `minConstantBufferAlignmentBytes`）。

`BatchAllocator` SHALL 用 `device->GetCapability().minUniformBufferOffsetAlignment` 对齐分配，MUST 不硬编码 256。

#### Scenario: 对齐由 caps 决定

- **WHEN** device 上报 `minUniformBufferOffsetAlignment = 16`，`Allocate(64)` 连续两次
- **THEN** 第二次返回的 offset 是 16 的倍数（而非 256 的倍数）

### Requirement: pack writer 返回 dynamic offset

pipeline 层 SHALL 提供 pack writer，把 per-object 结构体写入已分配 block 并返回该 block 的 pack offset；返回的 offset SHALL 可直接作为 `DrawItem::batchDynamicOffset` 传给 executor。

pack writer SHALL 用 `RgBlockDesc`/codegen struct 的 block size 校验（`static_assert(sizeof)`），MUST 不感知 rhi 层字节分配的内部实现。

#### Scenario: 写入并返回 offset

- **WHEN** 调用 pack writer 写入结构体 `T`（`sizeof(T)=64`）
- **THEN** 返回值为该帧 allocator 分配的、按 caps 对齐的绝对 offset，且该 offset 处的映射内存已写入 `T` 的内容

#### Scenario: offset 直通 executor

- **WHEN** pack writer 返回的 offset 写入 `DrawItem.batchDynamicOffset` 后 `Compile`+`Execute`
- **THEN** executor `BindResourceGroup(2, rg, 1, &offset)` 使用该 offset 作为 dynamic offset

