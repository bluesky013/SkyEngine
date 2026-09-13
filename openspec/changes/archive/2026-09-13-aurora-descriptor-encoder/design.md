## Context

`ResourceGroup` 当前通过 `Update(const std::vector<ResourceUpdateInfo>&)` 写入 descriptor。`ResourceUpdateInfo` 是一个「tag + union」胖结构（`ResourceWriteKind kind` + buffer/image/sampler 三组字段），后端 `VulkanResourceGroup::Update` / `D3D12ResourceGroup::Update` 遍历 vector、按 `kind` 分发到 `vkUpdateDescriptorSets` / D3D12 写路径。

问题：调用方要手工构造 `ResourceUpdateInfo`（易错）、每次 `Update` 构造 `std::vector`（堆分配）、不流畅。`DescriptorHeap::Update`（bindless tier2）也复用同一 `ResourceUpdateInfo`。

约束：3 后端（Vulkan / DX12 / Metal）；命名空间 `sky::aurora`；接口层不依赖 shader 模块。现有 encoder 模式（`GraphicsEncoder` / `ComputeEncoder` / `BlitEncoder`）由 `CommandBuffer::CreateXxxEncoder()` 返回接口类，后端各自实现。

## Goals / Non-Goals

**Goals:**

- 接口层 `DescriptorEncoder` 抽象 + 各后端原生实现（直接积累 native 写入，不经通用中间结构）
- `ResourceGroup::CreateEncoder()` 返回后端 encoder，`End()` 批量提交
- **移除 `ResourceUpdateInfo` + `Update(vector)`**，后端各自原生，类型安全、批量、避免每帧堆分配
- 迁移现有调用方（`GlobalRenderResources`、测试、`BatchPackWriter` 相关）与 `DescriptorHeap::Update`

**Non-Goals:**

- 不引入 root signature 1.1 静态 descriptor / descriptor heap 版本管理（后续优化）；Vulkan 侧本 change 即采用 `VkDescriptorUpdateTemplate`（见决策 3）
- 不实现 Metal `BindResourceGroup`（仍是 stub，随 `aurora-resource-group Metal phase`）
- 不做 descriptor set 的缓存/去重策略（重复写同一 binding 的合并，留作后续）
- 不改 bindless descriptor heap 的实际功能（tier2 未启用，只迁接口签名）

## Decisions

### 决策 1：`DescriptorEncoder` 是接口层抽象，后端原生实现

```cpp
// aurora/rhi/DescriptorEncoder.h
class DescriptorEncoder {
public:
    virtual ~DescriptorEncoder() = default;
    virtual void WriteBuffer(uint32_t binding, Buffer *buffer,
                             uint64_t offset, uint64_t range, uint32_t arrayElement = 0) = 0;
    virtual void WriteImage(uint32_t binding, Image *image,
                            ImageLayout layout, uint32_t arrayElement = 0) = 0;
    virtual void WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0) = 0;
    virtual void End() = 0;   // flush accumulated writes into the descriptor set
};
```

后端各自实现：
- **Vulkan** `VulkanDescriptorEncoder`：thin facade，`Write*` 写入 RG 的持久化 packed buffer 并置 dirty，`End()` dirty 时 `vkUpdateDescriptorSetWithTemplate`（详见决策 3）
- **DX12** `D3D12DescriptorEncoder`：`Write*` 直接写 root CBV / descriptor table 的 CPU handle（或记录待写）
- **Metal** `MetalDescriptorEncoder`：写 argument buffer（RG stub，先留空实现 + TODO）

**Why:** 去掉 `ResourceUpdateInfo` 中间层，写入直接落成 native 结构，避免「通用结构 → 后端再翻译」的二次搬运；同时公开 API 变成类型安全的 `WriteBuffer/WriteImage/WriteSampler`，杜绝 kind/字段错配。

**Alternatives considered:**
- *保留 `ResourceUpdateInfo`，仅在其上加链式 builder*：最小改动，但仍是通用中间结构，后端还要翻译一次，违背「直接对接各后端」的目标，被否决。
- *每个 binding 一个 `Write` 泛型方法（模板）*：类型安全但把后端实现拖进接口层模板，跨 DLL 边界复杂，被否决。

### 决策 2：`ResourceGroup::CreateEncoder()` 返回后端 encoder

```cpp
class ResourceGroup : public RefObject, public IDelayReleaseResource {
public:
    struct Descriptor { Shader *shader; uint32_t set; };
    virtual std::unique_ptr<DescriptorEncoder> CreateEncoder() = 0;  // 取代 Update(vector)
};
```

encoder 绑定到该 RG 的 descriptor set（Vulkan 拿 `GetNativeHandle()` 的 `VkDescriptorSet`；DX12 拿 root 参数）。`End()` 提交后 encoder 失效（或可复用）。

**Why:** 与 `CommandBuffer::CreateGraphicsEncoder()` 一致；encoder 必须知道目标 descriptor set（/root 参数），从 RG 上创建最自然。

**Alternatives considered:**
- *`Device::CreateEncoder(ResourceGroup*)`*：多一层参数，无收益，被否决。
- *encoder 脱离 RG，`End(group)`*：允许一个 encoder 写多个 RG，但 descriptor set 更新本来就 per-group，徒增复杂度，被否决。

### 决策 3：Vulkan 用 `VkDescriptorUpdateTemplate` + 持久化 packed buffer + dirty 去重

Vulkan 后端不在 encoder 里每帧重建 `VkWriteDescriptorSet` 数组，而是沿用旧引擎（`engine/render/backend/vulkan/`）已验证的模式：

- `VulkanShader::CreatePipelineLayout` 除了每个 set 的 `VkDescriptorSetLayout`，再为该 set 建一个 `VkDescriptorUpdateTemplate`（每个 binding 一条 `VkDescriptorUpdateTemplateEntry`，`offset/stride` 指向 packed buffer）
- `VulkanResourceGroup` 持一个**持久化 `mWriteInfos` packed 数组**（`DescriptorWriteInfo` = buffer/image info 的 union，size = 该 set 总 descriptor 数，一次分配）+ `mDirty`
- `VulkanDescriptorEncoder` 是 thin facade：`Write*` 直接写 `mWriteInfos[slot]` + 置 `mDirty`；`End()` 仅在 `mDirty` 时调 `vkUpdateDescriptorSetWithTemplate(set, template, mWriteInfos.data())` 并清 `mDirty`

部分更新天然支持：template 覆盖全部 binding，但 `mWriteInfos` 持久保存旧值，未写的 slot 保留旧值，`End()` 重 flush 时未改 binding 用的是旧值。

**template 支持情况**：`VK_KHR_descriptor_update_template` 在 **Vulkan 1.1 已进 core**（三个函数无需扩展）。Aurora 硬性要求 **Vulkan 1.3**（`VulkanDevice` 创建设备强校验 `dynamicRendering`/`synchronization2`），所以 template **始终可用，无运行时能力判断**。仍保留防御性 fallback：若 `vkCreateDescriptorUpdateTemplate` 失败（理论上仅 OOM），`End()` 回退到 `vkUpdateDescriptorSets` 朴素路径（同旧引擎）。

**DX12 / Metal**：DX12 无 template、写是即时的（descriptor heap `CreateCBV/SRV/Sampler`），encoder facade 引用 RG 的 `allocation` + `offsets` + `dynamicBindings`，`End()` 为 no-op；Metal 留 stub + TODO。DX12 的 shader-visible heap 隔离见决策 3b。

**Why:** layout 只记录一次（template），每帧只写 packed buffer + 一次调用 + dirty 去重，避免每帧重建 `VkWriteDescriptorSet` 的堆分配；且与旧引擎既有实现一致，不返工。

### 决策 3b：DX12 用 CPU-only staging + 每帧 shader-visible heap + bind 时 copy

D3D12 有「shader-visible heap 在执行期间不可被 CPU 覆盖」的硬约束，所以**不能**像当前实现那样让 encoder 直接写进单张 shader-visible heap（多帧 in-flight 会覆盖 GPU 仍在读的描述符）。正确结构：

- **CPU-only staging heap**（`D3D12_DESCRIPTOR_HEAP_FLAG_NONE`，持久 free-list）：`D3D12ResourceGroup` 在 `Init` 时 `Allocate` 一段持久区间，encoder 只写这里（**source of truth**，无 GPU 读、无覆盖顾虑）。
- **shader-visible ring**：`ringSize` 张 shader-visible heap（默认 3，≥ 渲染主循环的 in-flight 帧数），每张与 CPU heap 等大，**offset 1:1 映射**。
- **`D3D12DescriptorAllocator::BeginFrame(frameIndex)`**：`mCurrentFrame = frameIndex % ringSize`；由 `D3D12DeviceFrameContext::BeginFrame` 调用。
- **bind 时 copy**：`D3D12GraphicsEncoder::BindResourceGroup` 先 `d3dGroup->EnsureFrameCopy()`——若 `mDirty || mCopiedFrame != mCurrentFrame`，把该 RG 的 CPU 区间用 `CopyDescriptorsSimple` 拷到当前帧的 shader-visible heap（同 offset），再绑 shader-visible GPU handle。

语义：CPU heap 是唯一事实源（encoder 随便写）；每帧第一次 bind 时把脏/未复制过的区间 copy 到当前帧的 shader-visible heap，GPU 读当前帧那张。这样静态描述符（写一次）和每帧重写描述符都统一安全。

**Why:** 这是 D3D12 的标准「staging + ring + copy」模式（UE 等），一次性解决 in-flight 覆盖 hazard；offset 1:1 让 copy 是 O(1) 的 `CopyDescriptorsSimple`，无重映射。

**Alternatives considered:**
- *每帧重建 `VkWriteDescriptorSet` + `vkUpdateDescriptorSets`（上版设计）*：正确但每帧堆分配，且是旧引擎已淘汰的朴素路径，被本版替代。
- *`VK_KHR_push_descriptor`*：descriptor 直接写进 command buffer、跳过 descriptor set，但改的是「绑定模型」而非「写入封装」，且需 extension，超出本 change 范围。

### 决策 4：`ResourceUpdateInfo` 彻底删除，`DescriptorHeap::Update` 一并迁移

`DescriptorHeap::Update(allocation, writes)` 也去掉 `ResourceUpdateInfo`，改为接受一个 `DescriptorEncoder`（或独立 `WriteDescriptor` 接口）。bindless tier2 当前未启用（`CreateDescriptorHeap` 不支持时返回 nullptr），所以迁移只是接口签名对齐，无功能负担。

### 决策 5：迁移面

- `GlobalRenderResources::Init`：`mGroup->Update({write})` → `CreateEncoder()` + `WriteBuffer` + `End`
- `rhi/test/ResourceGroupTest.cpp`：`UpdateUniformBuffer` / `UpdateDynamicUniformBuffer` 等迁移
- `pipeline/test/ResourceTiersTest.cpp`：stable binding / range==0 测试迁移到 encoder
- `aurora-dynamic-ubo-pack` 的 stable binding 测试、`BatchPackWriter`（本身只用 `BatchAllocator`，不受影响，但其下游 RG 写入迁 encoder）

## Risks / Trade-offs

- **接口破坏（删 `Update` + `ResourceUpdateInfo`）** → 缓解：调用方仅 ~6 处，全部在本仓库内，一次性迁移；`aurora-resource-binding` spec 同步改
- **Vulkan template 函数未加载** → 缓解：`VulkanFunctions` 需补 `LOAD_DEVICE(vkCreateDescriptorUpdateTemplate)` / `vkDestroyDescriptorUpdateTemplate` / `vkUpdateDescriptorSetWithTemplate`（core 1.1，`vkGetDeviceProcAddr` 直接可拿）
- **Vulkan template fallback 路径** → 缓解：template 在 1.3 上 core 可用，fallback（`vkUpdateDescriptorSets`）仅在 `vkCreateDescriptorUpdateTemplate` 失败时触发，路径简单、可测试
- **DX12 动态绑定（root CBV）与静态 descriptor 写入路径不同** → 缓解：`D3D12DescriptorEncoder` 按 reflection 类型分发，静态走 descriptor table、动态走 root CBV（沿用现有 `D3D12ResourceGroup::Update` 逻辑）
- **Metal encoder 空实现** → 缓解：Metal RG 本就是 stub，encoder 留接口 + TODO，随 `aurora-resource-group Metal phase` 落地

## Migration Plan

1. 接口层：新增 `DescriptorEncoder.h`；`ResourceGroup.h` 加 `CreateEncoder()`、删 `Update()` + `ResourceUpdateInfo`
2. Vulkan 后端：`VulkanFunctions` 加载三个 template 函数；`VulkanShader` 为每个 set 建 `VkDescriptorUpdateTemplate`；`VulkanDescriptorEncoder`（thin facade）+ `VulkanResourceGroup::CreateEncoder`（持久化 packed buffer + dirty）
3. DX12 后端：`D3D12DescriptorEncoder` + 迁移
4. Metal 后端：`MetalDescriptorEncoder` 空实现 + TODO
5. 迁移调用方 + 测试；`DescriptorHeap::Update` 签名对齐
6. 删 `ResourceUpdateInfo` 收尾

## Open Questions

- **template 是否 per-shader 缓存？** 倾向：template 在 `VulkanShader::CreatePipelineLayout` 建、按 set 缓存（与 set layout 同生命周期），多个 RG 共享；`CreateEncoder()` 每次 `new` 轻量 facade（重状态在 RG 持久化 buffer），是否进一步池化 facade 留作性能验证后。
- **`WriteBuffer` 的 `range==0` 语义**：Vulkan 静态 UBO 允许 `VK_WHOLE_SIZE`；动态 UBO 沿用 `aurora-dynamic-ubo-pack` 的「显式 range 否则 assert」约定，encoder 内保持一致。
- **`DescriptorHeap` 是复用 `DescriptorEncoder` 还是独立 `WriteDescriptor` 接口？** 倾向：先复用 `DescriptorEncoder`（bindless 未启用，签名对齐即可），后续再按需拆分。
