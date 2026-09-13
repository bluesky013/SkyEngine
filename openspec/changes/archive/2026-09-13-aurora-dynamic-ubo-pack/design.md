## Context

Batch tier（set 2）在 `aurora-resource-tiers` 中已落地最小实现：`BatchAllocator`（`aurora/rdg/BatchAllocator.h`）管理单个 host-visible buffer，`Allocate(size)` 返回 256B 对齐的 offset，帧末 `Reset()`；`DrawItem.batchDynamicOffset` 携带该 offset，executor（`rhi/interface/src/rdg/Execute.cpp:73`）执行 `BindResourceGroup(2, rg, 1, &offset)`。

当前实现有三处隐患：

1. **无 in-flight 安全**：单 buffer + 帧末 `Reset()`，帧 N+1 覆盖帧 N 数据时 GPU 可能仍在读（`DeviceFrameContext` 已支持 `inflightNum` 帧 in-flight）。
2. **对齐硬编码 256**：`OFFSET_ALIGNMENT = 256` 是拍脑袋值，未从 device caps 查询。
3. **range 绑定错误**：`VulkanResourceGroup::Update`（`vulkan/src/VulkanResourceGroup.cpp:135`）在 `bufferRange == 0` 时用 `VK_WHOLE_SIZE`。对 `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`，range 必须是固定窗口（≤ block size），`VK_WHOLE_SIZE` 会让 `offset + dynamicOffset + range` 越界校验失败。

后端现状：Vulkan 的 dynamic offset 已走通（`vkCmdBindDescriptorSets` 带 dynamicOffsets）；DX12 已用 root CBV 实现 dynamic binding（`baseOffset + offset`，`dx12/src/D3D12Encoder.cpp:94`）；Metal 的 `BindResourceGroup` 仍是 stub（`aurora-resource-group Metal phase` 未做）。

约束：3 个后端（Vulkan / DX12 / Metal，无 GLES）；命名空间 `sky::aurora`；接口层不依赖 shader 模块（依赖方向 `shader → rhi` 单向，pack 逻辑放 rhi 接口层，pack writer 放 pipeline 层）。

## Goals / Non-Goals

**Goals:**

- 建立 **stable binding 契约**：dynamic UBO descriptor 每帧 `offset=0, range=blockSize` 绑定一次；帧内只有 per-draw dynamic offset 变化
- `BatchAllocator` 保持**纯线性分配器**，in-flight 由 `DeviceFrameContext` 维护 per-inflight-frame 的 pack buffer（packed pool）
- 对齐从 `DeviceCapability::minUniformBufferOffsetAlignment` 查询（默认 256 兜底）
- pipeline 层提供 **pack writer**：写 per-object 结构体 → 返回 dynamic offset
- Vulkan 后端动态 UBO 绑定固定 `range = blockSize`（不再 `VK_WHOLE_SIZE`）
- DX12 后端确认 `baseOffset + offset` 路径与 stable binding 语义一致

**Non-Goals:**

- 不实现帧内 free-list / defrag（线性分配器，帧内只增不减）
- 不做 GPU-side ring / device-local 环形（保持 CPU_TO_GPU host-visible）
- 不实现 bindless / descriptor heap（`aurora-resource-group tier2` 范畴）
- 不改 Global（set 0）/ Pass（set 1）tier
- 不实现 batch RG 的创建/绑定（由各特性如「材质→shader」自行创建；本 change 只提供 allocator + stable binding 契约 + 测试用例）
- Metal dynamic offset 完整实现（Metal ResourceGroup 仍是 stub，留待 `aurora-resource-group Metal phase`；本 change 只保证接口语义一致、不留 VK 特判）

## Decisions

### 决策 1：stable binding —— descriptor 绑 `offset=0, range=blockSize`，只变 dynamic offset

dynamic UBO 的正确绑定模型：

```
descriptor offset = 0
descriptor range  = blockSize   // 该 RG 对应 shader block 的固定大小（std140 后 16B 对齐）
dynamicOffset     = packOffset  // 每 draw 在 pack buffer 内的对齐偏移
```

Vulkan 校验要求 `offset + dynamicOffset + range <= bufferSize`，即最后一个 item 必须满足 `packOffset + blockSize <= capacity`。这意味着：

- `batchResourceGroup` 每帧对**当前帧的 pack buffer** 做一次 `Update({binding=0, buffer=packBuffer, offset=0, range=blockSize})`（一次/帧，非一次/对象）
- 帧内每 draw 只传 `batchDynamicOffset`，descriptor 本身稳定，杜绝 per-object 重建/重写 descriptor set

**Why:** 这是 dynamic UBO 相比「每帧重写 descriptor」的核心收益——descriptor set 数量与 CPU 更新成本与 draw 数解耦；`VK_WHOLE_SIZE` 非法，必须换成 `blockSize`。

**Alternatives considered:**
- *每 draw 重建 descriptor set（每个 item 独立小 UBO）*：descriptor set 数量 = draw 数，CPU 压力大，被否决
- *`range` 用整个 ring capacity*：Vulkan 不允许 range > `maxUniformBufferRange`，且 dynamic offset 语义会退化，被否决

### 决策 2：`BatchAllocator` 保持纯线性分配器，in-flight 由 FrameContext 维护 packed pool

`BatchAllocator` 退化为纯线性分配器，不感知「帧」、不做 frame index / slot 分区：

```cpp
class BatchAllocator {
    // Init(device, capacity)
    //   -> 创建单个 host-visible buffer；对齐内部从 device->GetCapability() 取
    uint32_t Allocate(uint32_t size);                       // 对齐分配，返回 offset，耗尽返回 UINT32_MAX
    void     Write(uint32_t offset, const void *data, uint32_t size);
    void     Reset();                                       // 清 cursor（帧结束）
    Buffer  *GetBuffer() const;
};
```

- 无 `inflightNum`、无 `BeginFrame(frameIndex)`——allocator 不感知「帧」。
- **in-flight 安全由 `DeviceFrameContext` 维护**：持有 `inflightNum` 个 `BatchAllocator`（每个 in-flight frame 一个），渲染主循环按帧取出一个写数据，帧的 fence 完成后回收并 `Reset()` 复用。
- 可选的 **packed pool**：单帧数据超过单个 buffer 容量时按需追加 buffer，帧结束整体回收（v1 只做单 buffer + 耗尽报错，扩容留作后续）。

**Why:** 把「帧」的概念从 allocator 上移到 FrameContext——它已经管理 in-flight 生命周期（command buffer / fence），pack buffer 归它管职责一致；allocator 是无状态纯内存分配，复用简单、可独立测试。

**Alternatives considered:**
- *allocator 内部 `frameIndex % inflightNum` 分槽（上版设计）*：把帧概念塞进 allocator，且 inflightNum 需与 frame context 同源，耦合；被本版替代。
- *单 buffer + 帧末 Reset（现状）*：不安全（多帧 in-flight 覆盖），被否决。
- *每 slot 独立小 buffer*：buffer 碎片化；且需要在 allocator 里维护 slot 生命周期，同样把帧概念塞进 allocator。

### 决策 3：`DeviceCapability` 新增 `minUniformBufferOffsetAlignment`

```cpp
struct DeviceCapability {
    uint32_t maxThreads = 1;
    uint32_t minUniformBufferOffsetAlignment = 256;  // 新增，默认 256 兜底
    bool anisotropyEnable = false;
    bool isUMA            = false;
};
```

各后端 `UpdateDeviceCaps()` 填真实值：

| 后端 | 来源 |
|---|---|
| Vulkan | `VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment` |
| DX12 | `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`（256） |
| Metal | `[device minConstantBufferAlignmentBytes]`（256） |

`BatchAllocator` 用该值对齐，替换 `OFFSET_ALIGNMENT`。block size 本身无需对齐（offset 对齐即可），但分配时对齐的是 offset。

**Why:** 不同厂商 / 移动 GPU 的 UBO offset 对齐要求不同（常见 16/64/256）；硬编码 256 在 16 对齐的设备上浪费空间，在更高要求的设备上可能非法。

### 决策 4：pack writer 放 pipeline 层

rhi 接口层的 `BatchAllocator` 保持「裸字节分配 + 写入」，不感知结构体布局。pipeline 层新增薄封装，把「RgBlockDesc → block size + per-field offset」与「结构体写入 + dynamic offset 返回」绑定：

```cpp
// pipeline 层（依赖 shader 模块的 ComputeFieldOffsets）
template <typename T>
uint32_t Pack(BatchAllocator &alloc, const T &data) {
    uint32_t off = alloc.Allocate(sizeof(T));
    alloc.Write(off, &data, sizeof(T));
    return off;   // == dynamic offset
}
```

实际落地可做成 `BatchPackWriter`（持有 `BatchAllocator&` + block size），配合 `RgBlockDesc` / codegen struct 的 `static_assert(sizeof)` 校验。

**Why:** rhi 层不能依赖 shader 模块（单向依赖约束），结构体布局知识属于 pipeline 层。字节分配留在 rhi 层，结构体感知留在 pipeline 层，边界清晰。

### 决策 5：Vulkan 动态 UBO 绑定固定 range

`VulkanResourceGroup::Update` 对 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC` 不再在 `bufferRange == 0` 时用 `VK_WHOLE_SIZE`，而是**要求调用方显式给出 range**；batch RG 初始化时写 `offset=0, range=blockSize`。

改动点：`ResourceUpdateInfo` 语义不变（`bufferRange` 已存在），只是 batch RG 的 Update 路径（由各特性/材质创建时）显式 `bufferRange = blockSize`；`VulkanResourceGroup::Update` 对 DYNAMIC 类型 + `range==0` 打印 warning（debug）或 assert，防止误用 `VK_WHOLE_SIZE`。

**Why:** 这是 stable binding 契约在后端的落点。`VK_WHOLE_SIZE` 对 static UBO 合法、对 dynamic UBO 非法，需区分。

### 决策 6：DX12 复用 root CBV `baseOffset + offset`；Metal 只保证语义一致

- **DX12**：已实现 root CBV dynamic binding（`baseOffset + offset`）。stable binding 下 `baseOffset = 0`、`offset = packOffset`，`SetGraphicsRootConstantBufferView(rootParam, gpuVA + packOffset)` 即达成等价语义。本 change 补齐 `D3D12ResourceGroup` 对 DYNAMIC 绑定的 range 记录（用于校验），不改 root CBV 机制。
- **Metal**：`BindResourceGroup` 仍是 stub，dynamic offset 本 change 不落地（`aurora-resource-group Metal phase` 负责）。设计上 Metal 用 `[renderEncoder setBufferOffset:atIndex:]` 达成动态偏移；本 change 只保证接口层 `minUniformBufferOffsetAlignment` 值已填，供后续使用。

**Why:** 不阻塞 stable binding 核心收益在 Vulkan 落地；DX12 只需接线；Metal 是已知未完成项，明确划出边界避免本 change 膨胀。

## Risks / Trade-offs

- **单帧数据超单 buffer 容量 → 分配失败返回 `UINT32_MAX`** → 缓解：v1 返回 `UINT32_MAX` + 调用方 assert/降级；pool 扩容（追加 buffer）留作后续；`Init` 提供 `capacity` 可调，默认给足（如 1MB）
- **`blockSize` 大于 buffer capacity** → 缓解：Init 时校验 `capacity >= blockSize`，否则报错
- **per-inflight-frame 回收依赖 frame fence 生命周期** → 缓解：回收由 `DeviceFrameContext` 在帧完成时驱动，与 command buffer 复用同一 fence 语义；文档写明「pack buffer 复用 MUST 与 frame context 的 in-flight 回收同生命周期」
- **Metal / DX12 能力不对称** → 缓解：能力对称是 `aurora-resource-group` 的既有债，本 change 不扩大范围，只保证接口层一致
- **block size 每 item 不同（多套 batch shader）** → 缓解：`range = blockSize` 是 per-RG（per-item）的，天然支持；但注意若同 RG 复用于不同 block size 会越界，文档写明「一个 batch RG 只对应一种 block layout」

## Migration Plan

无外部调用方，纯增量。落仓顺序：

1. 接口层：`DeviceCapability` 加 `minUniformBufferOffsetAlignment`；`BatchAllocator` 保持线性分配、对齐参数化（去掉 frame index）
2. 后端 `UpdateDeviceCaps` 三端填值
3. Vulkan：`VulkanResourceGroup::Update` 区分 DYNAMIC 类型 range 语义；测试中构造 batch RG 显式 `range = blockSize`
4. pipeline 层：pack writer；`ResourceTiersTest` 扩展到真实 block layout + stable binding
5. DX12 接线校验；Metal 记录 TODO（随 `aurora-resource-group Metal phase`）
6. per-inflight-frame 的 packed pool 维护随 `aurora-renderer` / `DeviceFrameContext` 落地（本 change 只交付 allocator 语义）

每一步独立 PR，可并行前 2 步。

## Open Questions

- **`Allocate` 返回绝对 offset 还是 buffer 内相对 offset？** 倾向：绝对 offset（直接当 dynamic offset），调用方无需关心帧/槽边界。
- **block size 是否 pad 到 alignment 的倍数？** 倾向：不 pad（offset 对齐即可，range 用真实 block size），减少内存浪费；若某后端要求 range 也对齐再补。
- **packed pool 是否 v1 落地？** 倾向：v1 只做单 buffer + 耗尽报错，pool 扩容（追加 buffer）留待 FrameContext/renderer 接入后。
- **per-inflight-frame 维护放 `DeviceFrameContext` 还是独立 pool 类？** 倾向：随 `aurora-renderer` 一起落到 FrameContext；本 change 只交付 allocator 语义 + 测试。
