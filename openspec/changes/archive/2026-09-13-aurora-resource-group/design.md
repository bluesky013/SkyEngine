## Context

Aurora 当前的 resource binding 链路完全断掉：

- `ResourceGroup::Descriptor` 是空 struct
- `ResourceGroupLayout::handlers` 是 private，无 setter，无 Init
- `Device::CreateSampler(const ResourceGroup::Descriptor&)` 是个 typo（应为 `CreateResourceGroup`），且所有 4 后端都返回 nullptr
- `VulkanGraphicsEncoder::BindResourceGroup` / `VulkanComputeEncoder::BindResourceGroup` 都是 `// TODO`
- `GraphicsPipeline::Descriptor` / `ComputePipeline::Descriptor` 没有任何 layout 字段，意味着 pipeline 创建时不知道要绑定什么

四个后端的 binding 模型差异极大：

| 后端 | 模型 |
|---|---|
| Vulkan | `VkDescriptorSetLayout` × N → `VkPipelineLayout`；`VkDescriptorSet` 来自 `VkDescriptorPool`；`vkCmdBindDescriptorSets(set, ...)` |
| DX12 | `ID3D12RootSignature`（root params 表达 binding slots）；descriptor heap (CBV/SRV/UAV + sampler 两类) + descriptor table；`SetGraphicsRootDescriptorTable(rootIndex, gpuHandle)` |
| Metal | argument buffer：`MTLArgumentEncoder` 编码到一个 `MTLBuffer`；`setVertexBuffer:offset:atIndex:` 把 argbuf 绑到 stage |
| GLES | 无 set 概念；按 binding 直接 `glBindBufferRange` / `glBindTextureUnit` / `glUniform*` |

aurora 需要一个统一中间层把这四套抽象出来。**目标是：调用方只面对 `ResourceGroupLayout` + `ResourceGroup` + `PipelineLayout` 三个概念，不感知后端差异。**

约束：
- Aurora 无现有调用方；零迁移
- 必须支持 push constants（VK / DX12 root constants / Metal setBytes / GLES uniform）
- 必须支持 array binding（count > 1）
- 必须支持动态偏移（VK_DESCRIPTOR_TYPE_*_DYNAMIC，对应 DX12 root CBV、Metal offset、GLES bindBufferRange offset）

## Goals / Non-Goals

**Goals:**
- 完整 ResourceGroupLayout / ResourceGroup / PipelineLayout 抽象与 4 后端落地
- BindResourceGroup 真正生效，单 group 单 binding 端到端绘制（采样一张纹理 + 一个 uniform buffer）
- Push constants 支持
- Array binding（count > 1）支持
- 动态偏移支持

**Non-Goals:**
- 不实现 bindless / variable count descriptor（VK_EXT_descriptor_indexing 高级特性）；本 change 只支持 fixed count
- 不实现 update-after-bind
- 不实现 descriptor pool 自动扩容的 GC 策略（先用足够大的池，溢出 assert）
- 不实现 RDG 自动绑定推导（RDG 后续 change）
- 不重写已有 PipelineState 的非 layout 部分

## Decisions

### 决策 1：ResourceGroupLayout 与 ResourceGroup 分离的两段式构造

```cpp
class ResourceGroupLayout : public RefObject {
public:
    struct BindingDesc {
        uint32_t                 binding;
        DescriptorType           type;
        uint32_t                 count        = 1;          // array size
        ShaderStageFlags         stages;
        DescriptorBindingFlags   flags;                     // 现有 enum，含 VARIABLE_COUNT
    };
    struct Descriptor {
        std::vector<BindingDesc> bindings;
    };
};

class ResourceGroup : public RefObject {
public:
    struct Descriptor {
        ResourceGroupLayout *layout = nullptr;
    };
    virtual void Update(const std::vector<ResourceUpdateInfo> &writes) = 0;
};
```

**Why:** Layout 是不可变 + 可复用；Group 是 layout 的实例，可被多次 Update。VK / DX12 / Metal 都符合这个模型。

### 决策 2：PipelineLayout 是必填一等公民

```cpp
class PipelineLayout : public RefObject {
public:
    struct Descriptor {
        std::vector<ResourceGroupLayout*> groups;          // by set index
        std::vector<PushConstantRange>    pushConstants;   // 已有结构
    };
};

struct GraphicsPipeline::Descriptor {
    PipelineState   *state    = nullptr;
    Shader          *shader   = nullptr;
    PipelineLayout  *layout   = nullptr;        // ← 新增必填
    AttachmentFormat format;
};

struct ComputePipeline::Descriptor {
    Shader         *cs     = nullptr;
    PipelineLayout *layout = nullptr;           // ← 新增必填
};
```

**Why:** Pipeline 必须知道签名（Vulkan / DX12 强约束）；Metal / GLES 也需要（用于 layout 翻译 / uniform location 解析）。

### 决策 3：ResourceUpdateInfo 用 union + type 区分

```cpp
enum class ResourceWriteKind { BUFFER, IMAGE, SAMPLER, COMBINED_IMAGE_SAMPLER };

struct ResourceUpdateInfo {
    uint32_t          binding;
    uint32_t          arrayElement = 0;     // for array binding
    ResourceWriteKind kind;
    union {
        struct { Buffer *buffer; uint64_t offset; uint64_t range; } buf;
        struct { Image  *image;  ImageLayout layout; }              img;
        struct { Sampler *sampler; }                                smp;
        struct { Image *image; Sampler *sampler; ImageLayout layout; } combined;
    };
};
```

调用方批量传 writes 一次性 Update。

**Why:** VK descriptor write 是 batched；DX12 是把多个 descriptor 写到 heap region；Metal 是依次 encode 到 argument buffer——批量 API 都能 map。

### 决策 4：Encoder::BindResourceGroup 携带动态偏移

```cpp
virtual void BindResourceGroup(
    uint32_t set,
    ResourceGroup *group,
    uint32_t numDynamicOffsets = 0,
    const uint32_t *dynamicOffsets = nullptr) = 0;
```

dynamic offsets 的索引顺序与 layout 中 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC` 出现的顺序一致（与 VK 一致）。

### 决策 5：Push constants 通过 Encoder 接口

```cpp
class GraphicsEncoder { ...
    virtual void PushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data) = 0;
};
class ComputeEncoder { ...
    virtual void PushConstants(uint32_t offset, uint32_t size, const void *data) = 0;
};
```

后端翻译：
- VK: `vkCmdPushConstants`
- DX12: `SetGraphicsRoot32BitConstants`（push constants 在 root signature 里申请一个 inline constant root param）
- Metal: `setVertexBytes:length:atIndex:`（占用 buffer slot 30，与 argument buffer 错开）
- GLES: 内部维护一个 SSBO/UBO 模拟，或拍平成 uniform 写入

### 决策 6：4 后端的 layout / group 翻译

| 概念 | Vulkan | DX12 | Metal | GLES |
|---|---|---|---|---|
| ResourceGroupLayout | `VkDescriptorSetLayout` | 一段 root signature 子树 + descriptor heap range | `MTLArgumentEncoder` + slot map | binding → type/count/stage 数组 |
| PipelineLayout | `VkPipelineLayout` | `ID3D12RootSignature` | argument buffer slot 表 + push constants buffer slot | uniform / texture unit 映射表 |
| ResourceGroup | `VkDescriptorSet`（来自 device 内部 pool） | descriptor heap 内一段连续区域 + sampler heap 区域 | `MTLBuffer`（argument buffer） | `binding → resource ptr` 映射表 |
| Update | `vkUpdateDescriptorSets` | `CopyDescriptors` 到 heap region | `[encoder setBuffer:atIndex:]` 编码 | 直接写映射表，BindResourceGroup 时 flush |
| BindResourceGroup | `vkCmdBindDescriptorSets` | `SetGraphicsRootDescriptorTable` × N | `setVertexBuffer:argbuf,offset,atIndex` | 按映射表 `glBindBufferBase/Range` / `glBindTextureUnit` |

DX12 descriptor heap 管理：
- Device 持有两个全局 heap：CBV/SRV/UAV heap、SAMPLER heap
- 每个 ResourceGroup 在 heap 中预留连续区域（pool 分配，固定大小，溢出新建子 heap）
- BindResourceGroup 切换 heap 时调 `SetDescriptorHeaps`（每帧 ≤ 1 次切换）

Metal argument buffer：
- ResourceGroup = 一个 `MTLBuffer`，由 device pool 分配
- Update 时 lazily 持有 `MTLArgumentEncoder` 编码资源指针到 buffer
- BindResourceGroup 把 argbuf 绑到对应 stage 的 buffer slot（set index 直接映射到 slot N）；同时调用 `[encoder useResource:usage:]` 让 driver 知晓 residency

### 决策 7：DescriptorBindingFlagBit 暂不支持 VARIABLE_COUNT

枚举里已有 `VARIABLE_COUNT`，但本 change scope 内 array count 必须固定。Layout 校验阶段对带 VARIABLE_COUNT 的 binding 报错。VARIABLE_COUNT / bindless 是后续 change。

### 决策 8：内部 descriptor pool 策略

VK / DX12 descriptor pool 设计：
- Device 内部维护多个 pool；每个 pool 容量按 layout 类型计数（例如：每 pool 允许 1024 个 set，含 4096 个 sampled image、1024 个 uniform buffer）
- `CreateResourceGroup` 从首个未满 pool 分配；满了开新 pool
- 不实现自动 GC；ResourceGroup 析构时把 set / heap 区域归还本 pool（free list）

## descriptor heap 方向（tier1 pool + tier2 heap，按 VK_EXT_descriptor_heap）

### 核心模型

| tier | 能力 | typed（`layout(set, binding)`） | untyped（`layout(descriptor_heap)`） |
|---|---|---|---|
| tier1 | 无 `VK_EXT_descriptor_heap` | `VkDescriptorPool` + `VkDescriptorSet` | 不可用 |
| tier2 | 有 `VK_EXT_descriptor_heap` | heap + `VkShaderDescriptorSetAndBindingMappingInfoEXT`（SPIR-V 映射） | heap + untyped pointer（`VK_KHR_shader_untyped_pointers` + `SPV_EXT_descriptor_heap`） |

tier2 里 **typed 与 untyped 都走同一套 heap**，不再用 pool；pool 只是 tier1 的兜底。

### 决策 9：DescriptorHeap = 一个对象管两个 backing buffer（resource + sampler）

```cpp
class DescriptorHeap : public RefObject {
public:
    struct Descriptor {
        uint32_t maxTextures = 0;   // sampled/storage image
        uint32_t maxBuffers  = 0;   // uniform/storage buffer
        uint32_t maxSamplers = 0;
    };
    struct Allocation {
        uint32_t texFirst = 0, texCount = 0;
        uint32_t bufFirst = 0, bufCount = 0;
        uint32_t smpFirst = 0, smpCount = 0;
    };

    virtual Allocation Allocate(const Descriptor &) = 0;
    virtual void Free(const Allocation &) = 0;
    // 写 descriptor blob（vkWriteResourceDescriptorsEXT）到索引段
    virtual void Update(const Allocation &, const std::vector<ResourceUpdateInfo> &writes) = 0;
};
```

内部两个 backing `VkBuffer`（resource heap + sampler heap，`VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT` + `SHADER_DEVICE_ADDRESS`），分别 `vkCmdBindResourceHeapEXT` / `vkCmdBindSamplerHeapEXT`。分配按 per-type stride（`image/buffer/samplerDescriptorSize`）+ `*DescriptorAlignment` + `resourceHeapAlignment`/`samplerHeapAlignment` + reserved range（`min*ReservedRange`）。

### 决策 10：typed ResourceGroup 在 tier2 也走 heap（mapping）

typed `ResourceGroup` 接口不变；backend 实现分档：

- tier1：`VkDescriptorSet`（pool）。
- tier2：heap 里一段 region + `VkShaderDescriptorSetAndBindingMappingInfoEXT`（set/binding → heap offset，pipeline/shader 创建时提供）。

调用方无感；`ResourceGroup` 恒可用，`DescriptorHeap` 仅 tier2。

### 决策 11：mapping source 先只做 HEAP_WITH_PUSH_INDEX

material index 走 push data（`VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT`）。`CONSTANT_OFFSET` / `INDIRECT_INDEX` / `INDIRECT_INDEX_ARRAY` / inline 系列（PUSH_DATA / PUSH_ADDRESS / INDIRECT_ADDRESS）/ shader-record 留后续。接口留 mapping source 枚举位，本轮只实现 push index。

### 决策 12：untyped 不支持 combined image sampler

`VK_EXT_descriptor_heap` 的 untyped 模型不支持 `COMBINED_IMAGE_SAMPLER`（combined 跨两个 heap，untyped 无法分别设 image/sampler 的 array stride）。`DescriptorHeap` 的 untyped 路径 SHALL 拒绝 combined，只允许分离的 sampled image + sampler；typed 路径（`ResourceGroup`）仍支持 combined（tier1 pool 原生、tier2 mapping 用 `samplerHeapOffset` / `useCombinedImageSamplerIndex`）。

### 决策 13：接口先定，实现拆 change

- 本 change：定接口（`DescriptorHeap` + 能力门 + mapping source 枚举）+ tier1 pool 收尾（DX12/Metal 补齐 `ResourceGroup`）。
- tier2 heap 实现拆后续 change：Vulkan（`VK_EXT_descriptor_heap`）→ DX12（native `ID3D12DescriptorHeap` + SM6.6 `ResourceDescriptorHeap[]`）→ Metal（argument buffer）。

### Vulkan descriptor heap 模型（VK_EXT_descriptor_heap）——记录

1. **两个 heap**：resource heap + sampler heap，各是 `VkBuffer`（`VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT` + `SHADER_DEVICE_ADDRESS`）。
2. **descriptor = 不透明 blob**，per-type stride（`sampler/image/bufferDescriptorSize`，精确值 `vkGetPhysicalDeviceDescriptorSizeEXT`）。
3. **写 descriptor**：`vkWriteResourceDescriptorsEXT`（写 host 内存或 `pDescriptors->address` 直指 heap）；**无 `VkImageView`**，image 直接喂 `VkImageViewCreateInfo`。
4. **绑 heap**：`vkCmdBindResourceHeapEXT` + `vkCmdBindSamplerHeapEXT`（heapRange + reservedRange）。
5. **无 `VkDescriptorSetLayout` / `VkPipelineLayout`**：typed 用 `VkShaderDescriptorSetAndBindingMappingInfoEXT`（pipeline/shader 创建时映射 set/binding → heap offset）；untyped 用 `VK_KHR_shader_untyped_pointers` + `SPV_EXT_descriptor_heap`。
6. **mapping source**：`VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT` 等，决定 shader 索引怎么落到 heap offset。
7. **无 dynamic UBO**（push index / indirect index / buffer device address 替代）；**null descriptor**（`nullDescriptor` feature）；**alignment**（heap + per-descriptor）。

### 后端映射表

| 概念 | Vulkan（tier2） | DX12（tier2） | Metal（tier2） |
|---|---|---|---|
| DescriptorHeap | 两个 `VkBuffer`（resource/sampler）+ `vkWriteResourceDescriptorsEXT` + `vkCmdBindResourceHeapEXT`/`BindSamplerHeapEXT` | `ID3D12DescriptorHeap`（CBV/SRV/UAV + sampler） | argument buffer（`MTLBuffer`）+ `MTLArgumentEncoder` |
| typed（ResourceGroup） | heap region + `VkShaderDescriptorSetAndBindingMappingInfoEXT` | root descriptor table（heap 区间） | argument buffer 区间 |
| untyped（DescriptorHeap） | untyped pointer + `SPV_EXT_descriptor_heap` | SM6.6 `ResourceDescriptorHeap[]` | Metal bindless / argument buffer 索引 |
| 逐 draw 索引 | `HEAP_WITH_PUSH_INDEX`（push data） | root constant / push 里的 index | push / `setBytes` 里的 index |

## Risks / Trade-offs

- **DX12 root signature 设计：把每个 set 映射成一个 root descriptor table** → 缓解：限制 group 数 ≤ 4（VK 也常见），剩余 root slot 留给 push constants + dynamic CBV
- **Metal push constants 用 `setBytes:` 占用 slot 30** → 缓解：在 PipelineLayout 中预留 slot 30，文档化与 argument buffer 的 slot 冲突
- **GLES 没有真正的 set，所有 binding 都是 global** → 缓解：在 layout 校验阶段计算所有 set 内 binding 不冲突（同 binding index 不能在不同 set 里出现）；如冲突，编译期报错
- **descriptor pool 不 GC 可能导致 fragmentation** → 缓解：本 change 用固定大小 pool；监测 hits/misses，未来 change 加 LRU
- **重命名 `CreateSampler(ResourceGroup::Descriptor)` 与 `aurora-quick-fixes` 重叠** → 缓解：aurora-quick-fixes 先 land 重命名（小改动）；本 change 在已 rename 的 API 上加实现

## Migration Plan

依赖关系：
- 依赖 `aurora-quick-fixes`（CreateResourceGroup 重命名）
- 依赖 `aurora-queue-submit-present`（端到端测试需要 Submit）
- 与 `aurora-encoder-barriers` 互不依赖（barrier 不影响 binding API）

落仓顺序：
1. PipelineLayout / ResourceGroupLayout 接口先合
2. ResourceGroup + Update 接口
3. Vulkan 后端跑通端到端
4. DX12 跟上（同步把 RootSignature 写实，与 PipelineState 完成度一起补）
5. Metal / GLES
6. 测试同步合入

## Open Questions

- **是否在接口层提供 `ShaderReflection` 自动生成 layout？** 倾向：本 change 只做手写 layout；反射作为独立 change（SPIRV-Cross / D3D12 root sig 反射 / Metal autogen）。
- **typed（`ResourceGroup`）在 tier2 的 heap 映射，其 set/binding → heap offset 的映射信息由谁提供？** `VkShaderDescriptorSetAndBindingMappingInfoEXT` 在 pipeline/shader 创建时给；aurora 侧是沿用 `ResourceGroupLayout` 生成，还是引入 `ShaderReflection` 生成。倾向：先沿用 `ResourceGroupLayout` 手写，反射后续。
