## Context

新 frame IR（`FrozenFrameGraph`）把 pass 分成 copy、raster、fullscreen、compute、custom。copy/raster 的读取范围、内容定义和 attachment 状态都来自图声明，因此各后端能用自己的资源状态跟踪器推导 barrier。`FrameComputePass` 目前只有 shader/groups，`FrameCustomPass` 只有 name/sideEffect；两者没有任何 buffer/image 声明，`AnalyzeFrameGraph` 在分配和编译前直接返回 Unsupported（`FrameGraphAnalysis.cpp:208`）。

老 RDG 已有可复用的模型：`ComputePassBuilder::Read/Write`、`CustomPassBuilder::Read/Write`，`RenderGraph::DeriveBarriers` 按声明统一推导 front barriers。新 IR 缺的不是算法，而是声明入口。本 change 把声明补回新 IR，并让各后端编译器把 compute/custom 接到已有的状态跟踪上，而不是让 pass 回调自己发 barrier。

约束：
- 新 IR 的 buffer 状态是粗粒度 `CopyBufferState`，image 状态是 `ImageLayout`；现有后端对 buffer 做整资源 barrier，对 image 按 mip/layer 子资源跟踪。
- 规范要求 execute 回调不得调用 `PipelineBarrier`（`openspec/specs/aurora-rdg/spec.md:429`），Custom 只能使用声明访问和获准 executor（`:220`）。
- 新 IR 要求各后端自行组织完整 Compile 和同步（`:575`），公共代码只做声明校验与纯分析。
- Metal 已有最小 offscreen serial frame context；真实设备验收受 `aurora-renderer-platform-parity` 的 P02 环境约束。

## Goals / Non-Goals

**Goals:**
- 给 `FrameComputePass` / `FrameCustomPass` 增加显式 buffer/image/sampler 声明和访问模式，使 barrier、内容、lifetime、culling 全部由图声明推导。
- 扩展 buffer 边界状态以表达 shader read 和 unordered access，并保持导入/导出校验完整。
- Vulkan（RenderPass + Dynamic Rendering）、DX12、Metal 在各自 compiler 中执行 compute/custom，并复用自身资源状态跟踪。
- Custom 提供 RHI 级串行逃生门：回调拿到 command buffer 与已解析资源，但不得插入 barrier；不支持的后端/能力显式 Unsupported。
- 用读回测试覆盖 compute 写 storage buffer/image 后由 copy/raster 消费、custom 回调写入、barrier 顺序、未定义内容拒绝。

**Non-Goals:**
- 不支持 async compute、跨队列同步或 compute 与 graphics 并行提交。
- 不支持后端专属的 custom 二进制契约（如 DLSS SDK 直接接管原生 encoder）；Custom 只承诺 RHI 级命令。
- 不把 compute 编码并行化到 worker；本 change 中 compute/custom 都在 coordinator 串行编码，后续需要时再单独优化。
- 不改老 RDG 的 `RenderGraph`/`DeriveBarriers` 行为；只迁新 IR。
- 不增加 storage 格式、跨队列 ownership transfer 或新的 image 格式。

## Decisions

### 决策 1：显式声明访问，而不是回调内 barrier

新增与 raster binding 同构的声明：

```cpp
enum class FrameAccessMode : uint8_t { READ, WRITE, READ_WRITE };
enum class FrameBufferBindingKind : uint8_t { REFLECTED, UNIFORM, STORAGE };
enum class FrameImageBindingKind : uint8_t { REFLECTED, SAMPLED, STORAGE };

struct FrameBufferBinding {
    uint32_t         set = 0, binding = 0;
    RasterBufferRange range;              // 复用 graph buffer + offset/size
    uint32_t         dynamicOffset = 0;
    FrameAccessMode  access = FrameAccessMode::READ;
    FrameBufferBindingKind kind = FrameBufferBindingKind::REFLECTED;
};

struct FrameImageBinding {
    uint32_t         set = 0, binding = 0;
    GraphImageHandle image;
    RasterImageView  view;                // 复用 mip/layer 选择
    FrameAccessMode  access = FrameAccessMode::READ;
    FrameImageBindingKind kind = FrameImageBindingKind::REFLECTED;
};

struct FrameComputePass {
    ShaderPtr                       shader;
    uint32_t                        groupsX = 1, groupsY = 1, groupsZ = 1;
    std::vector<FrameBufferBinding> buffers;
    std::vector<FrameImageBinding>  images;
    std::vector<RasterSamplerBinding> samplers;
    bool                            sideEffect = false;
};
```

`FrameCustomPass` 使用同一组 `buffers/images`，外加：

```cpp
class FrameCustomPassEncoder {
public:
    virtual ~FrameCustomPassEncoder() = default;
    virtual FrameOperationResult Encode(FrameCustomPassContext &) const noexcept = 0;
};

struct FrameCustomPassContext {
    CommandBuffer              &command;
    std::span<Image *const>     images;   // 与声明同序
    std::span<Buffer *const>    buffers;  // 与声明同序
};

struct FrameCustomPass {
    std::string                                 name;
    std::shared_ptr<const FrameCustomPassEncoder> encoder;
    std::vector<FrameBufferBinding>             buffers;
    std::vector<FrameImageBinding>              images;
    bool                                        sideEffect = false;
};
```

**Why:** 声明让 `AnalyzeFrameGraph` 能计算读取范围、内容定义、lifetime 和依赖，后端能调用已有的 `ImageBarrier`/`BufferBarrier`。回调内 barrier 会绕过 culling/alias/parallel 分析，且与规范冲突。

**Alternatives considered:**
- *回调内发 barrier*：实现最快，但无法保证顺序、内容证明和跨后端一致，否决。
- *只靠 shader reflection*：Custom 没有 shader；reflection 也不提供 range/内容声明，否决。
- *给每种 pass 单独跟踪器*：会把声明问题复制到每个后端，正是当前 debt 的成因，否决。

### 决策 2：扩展 buffer 状态，image 复用现有 layout

`CopyBufferState` 增加：

```cpp
enum class CopyBufferState : uint8_t {
    COMMON, HOST_WRITE, COPY_SOURCE, COPY_DESTINATION, HOST_READ, RASTER_READ,
    SHADER_READ, UNORDERED_ACCESS
};
```

映射规则：
- Compute 由 reflection 的 descriptor 类型决定原生绑定与状态；access 决定内容依赖和定义。uniform buffer 在 DX12 使用 `VERTEX_AND_CONSTANT_BUFFER`，Vulkan 使用 shader read；storage buffer 始终使用 UAV/storage 状态，storage image 始终使用 `GENERAL`，包括只读 storage 声明。
- sampled image 只能 READ，使用 `SHADER_READ_ONLY`。Custom 必须显式声明 UNIFORM/SAMPLED/STORAGE；kind 选择原生状态，access 独立决定内容读写。STORAGE + READ 仍使用 storage/UAV，不等价于 SRV StructuredBuffer。
- 同状态写仍需建立依赖；DX12 连续 UAV 访问发出 UAV barrier，不能只在状态变化或 alias 切换时同步。

Buffer 仍按整资源跟踪（现有 `VulkanGraphResources::BufferBarrier` 已是 whole-size），image 按 mip/layer 子资源跟踪；声明中的有效 offset（base offset + dynamicOffset）与 size 用于内容证明和 lifetime，不引入 buffer 子范围状态机。

**Why:** 不改变现有后端跟踪器的粒度，避免在三个后端同时引入 buffer 子范围状态。代价是 compute 相邻 pass 之间可能出现整 buffer barrier，首版接受。

### 决策 3：Compute 用 reflection 校验，Custom 以声明为准

- Compute：按 `shader->Reflection()` 校验每个 set/binding；sampler 必须匹配 SAMPLER，uniform buffer 只能 READ，storage buffer 可 READ/WRITE/READ_WRITE，sampled image 只能 READ，storage image 可任意。重复 set/binding、缺失声明、声明了 shader 未使用的资源或缺失 shader 都失败。
- Compute 默认 REFLECTED，显式 kind 必须匹配 reflection；Custom 必须显式填写 kind，UNIFORM/SAMPLED 只允许 READ。
- Custom：没有 reflection，声明即契约；encoder 为空、声明为空且 `sideEffect=false`、非法 range/view/access 均失败。
- WRITE 承诺完整覆盖声明范围；READ_WRITE 还要求该范围已有内容，框架不从 shader 指令或 dispatch 组数推断覆盖。两者写声明都会 `defines` 对应内容范围；读声明要求内容已定义，规则与 copy/raster 相同。buffer 使用加上 dynamicOffset 后的有效范围，并在加法前校验上界以避免溢出。
- Compute/Custom storage image 均要求单 sample，现有全局 MSAA usage 校验仅允许纯 RENDER_TARGET/DEPTH_STENCIL，已排除 STORAGE。Compute view 必须恰好一个 mip；mipCount=0 先归一化为剩余 mip，不能把 native UAV 未覆盖的 mip 证明为已定义。Custom 的声明范围仍可覆盖多个 mip。
- 声明与 Custom 回调资源数组保持原顺序。原生 descriptor 按 set/binding 匹配；Vulkan/Metal 的 dynamic offsets 单独按 binding 升序生成，不能按声明插入顺序传递。

**Why:** Compute 有 shader 可校验，能尽早发现错误声明；Custom 是逃生门，无法静态校验，只能把声明作为唯一权威并用测试约束。

### 决策 4：Compute/Custom 在 coordinator 串行编码

- Vulkan RenderPass/Dynamic Rendering：compute/custom 作为独立 logical pass，前后用声明推导 barrier；RenderPass 模式在进入 compute 前结束当前 native scope，之后按 attachment 重新开 scope，不跨 compute 合并。
- DX12：在 DIRECT list 上做 state transition、dispatch/自定义命令；compute/custom 与其他 pass 之间按声明插入 transition。
- Metal：compute 用 compute encoder，custom 用当前命令缓冲；encoder 边界本身提供大部分同步，跨 encoder 的 hazard 用显式 memory barrier 或额外 encoder 边界处理。
- EncodeTaskGraph 中 compute/custom 是 coordinator-only 节点，Custom 不声明并行；GPU 顺序仍由声明访问决定，不因 CPU 串行改变。

**Why:** 首版避免为 compute 引入 descriptor/state 的并行编码分片；Custom 规范本来就要求默认串行。Compute 的并行分片可以后续在不改 IR 的前提下单独做。

**Alternatives considered:**
- *Compute 按 raster 的方式切 fragment 并行编码*：需要新的 descriptor 冻结和 state 归属规则，超出本 change，否决。
- *Custom 使用后端原生 encoder 回调*：会把原生类型带进公共 IR，且各后端契约不同，首版否决。

### 决策 5：Metal 同源实现，实机验收复用 parity P02

Metal compiler 按同一套声明生成 compute/custom plan；Windows 只验证公共契约和 Vulkan/DX12，Metal 的 macOS 编译与真实设备调度/读回验收沿用 `aurora-renderer-platform-parity` 的 P02 环境，不新建重复平台 change。若 Metal 能力不足，编译显式返回 Unsupported。

**Why:** 保证三后端接口一致，同时不假装在 Windows 上验证了 Metal GPU 行为。

### 决策 6：收编现有 change 的重复范围

- `aurora-renderer-platform-parity` 4.20 保留“pass kind 能力范围/跨平台验收”，其中 Compute/Custom 的 IR 与 Vulkan/DX12 实施改由本 change 负责；Metal 实机验收仍留在 4.20。
- `aurora-renderer-extensions` E2 的 Compute/Custom 条目指向本 change，避免同一需求两处排期。

### 决策 7：抽取纯校验与后端私有辅助，明确 GPU 保活

- `ResolveFrameBufferRange` / `ResolveFrameImageView` 只解析声明，`ResolveComputeDynamicOffsets` 只排序已验证的绑定；原生 usage、alignment、dispatch limits 和状态映射仍由后端负责。
- DX12 的 compute PSO/descriptor 构建收敛到私有 `ResolveCompute`；Vulkan 两 compiler 共用 compute/custom step 的编码及 custom 资源解析，各自保留完整 scope/同步编排。
- 公共 `FrameTransientPool` 对 live compute 和 raster 一样收集 shader/sampler 引用。Accepted 后 CPU 图可释放，GPU bindings 随 slot 保活至完成或安全 teardown。
- Custom 失败停止后续编码并统一收尾；具有可失败结束接口的后端仍保持 DeviceLost 优先。Metal End 为 void 空实现，不存在可保留的 End 错误，统一路径属于防御性完善。
- Custom 返回前须结束其创建的 encoder，不得保留 command/context 引用；callback 及其拥有的 GPU 对象随提交 payload 保活至安全退休。
- 本次不增加 target、不迁移目录；整帧 coordinator-only 调度和 compute PSO 跨帧缓存另行优化。

### 决策 8：声明冲突、副作用与 Metal 校验

- 同 pass 的同一物理 buffer 禁止混合 UNIFORM/STORAGE，即使字节范围不重叠；同 image 的重叠 mip/layer 禁止混合 SAMPLED/STORAGE。相同 kind 重复绑定和不重叠 image views 允许。
- sideEffect=true 的无资源 pass 仍执行。NoWork 同时检查 livePasses 与 used resources，保留副作用和仅有资源边界转换的图。
- Vulkan storage/general 同步保守使用 ALL_COMMANDS，覆盖 Custom 的 graphics/compute 前后依赖；本轮不增加公共 stage 字段或第二套状态跟踪器。
- groupsX/Y/Z 全部非零，零值返回 INVALID_ARGUMENT；未来 no-op 支持必须同步调整内容定义、依赖、裁剪，不能仅移除检查。
- Metal pipeline 创建前检查 exact device、compute shader、有效 buffer 范围、实际 usage/memory 枚举及 uniform 偏移。uniform 沿用 Metal DeviceCapability 公布的保守契约，不称其为硬件最低值。
- Metal memory 校验只拒绝非法枚举，不禁止 CPU_ONLY/CPU_TO_GPU/GPU_TO_CPU：当前 ToMetalBufferOptions 将 GPU_ONLY 映射为 private，其他合法类型映射为 shared。usage、原生对象、范围与对齐仍须校验。
- threadGroupSize 各轴按 device.maxThreadsPerThreadgroup 校验；pipeline 创建后按 maxTotalThreadsPerThreadgroup 检查乘积，除法先于乘法防溢出。不能用局部线程限制代替组数限制，不新增 groups×threads 的 32 位限制。
- Metal 创建 pipeline 时请求 argument/buffer type reflection，只保存 active buffer 的 slot/alignment；复用 BuildMetalMSLSlotLayout 映射 binding。反射缺失或 active buffer 无有效 alignment 返回 UNSUPPORTED，成功反射中的 inactive/优化移除 binding 不施加类型对齐限制，不长期保留原生反射对象。
- 整份反射缺失不能证明某 binding 已优化移除；带 buffer 的 Compute 保守拒绝，不猜测 alignment。无 buffer 的 Compute 不依赖这份 buffer 元数据。
- base offset 和 dynamic offset 分别按 native alignment 验证，storage 不硬编码 16 字节。Custom 没有固定 pipeline，框架验证声明/usage，实际绑定类型与对齐由 callback 保证。
- image format/type/extent/mip/layer/sample/usage 与真实 MTLTexture 对照。Compute storage view 限制单 mip/X1，Custom storage 仍要求 X1，但访问声明可跨多个 mip。
- Metal 原生预检的非法参数返回 INVALID_ARGUMENT；明确缺失能力/元数据返回 UNSUPPORTED；native pipeline 创建失败返回 COMPILE_FAILED。Windows 不承担 Metal 编译/实机验收。

## Risks / Trade-offs

- [Custom 回调可能违反声明] → 文档与测试明确契约；首版不做运行时拦截，无法从 RHI 命令流反推真实访问。
- [整 buffer barrier 过度同步] → 首版接受；后续如出现性能问题，再引入 buffer 子范围状态，不改变声明 API。
- [Vulkan RenderPass 模式无法跨 compute 合并 scope] → 预期行为；读回测试对照 Dynamic Rendering，确保内容一致。
- [Metal 代码无法在 Windows 验证] → Windows 验证公共契约，macOS 编译与实机读回由 parity P02 跟踪；失败时明确 Unsupported，不静默降级。
- [buffer 状态扩展影响导入/导出校验] → 同步更新所有 `> RASTER_READ` 校验和导入测试；新增状态必须能在三后端映射，否则编译期报 Unsupported。
- [Compute 串行编码降低 GPU 喂送效率] → 首版以正确性优先；并行化作为独立后续，不影响 IR。

## Migration Plan

- kind 默认 REFLECTED，Compute 保持反射推导；Custom 调用方迁移为显式 kind，遗漏时拒绝。现有 copy/raster 图不变。
- `AnalyzeFrameGraph` 对 compute/custom 的 Unsupported 分支改为声明校验；保留既有通用图校验的 COMPILE_FAILED，非零 dispatch 和有效 buffer range 的参数错误返回 INVALID_ARGUMENT，测试同步更新。
- 回滚只需还原 IR 与后端 compiler 改动；已冻结图的 ABI 不跨进程使用，无持久化兼容问题。

## Open Questions

- Custom encoder 是否需要 `Device&`（用于查询能力）还是仅 `CommandBuffer&` 足够？首版按需在 context 中补充，不提前扩大接口。
- Compute 的 set/binding 是否允许与 raster 共用同一套 descriptor 缓存 key？首版复用绑定结构，缓存 key 在实现时按 reflection 确定。
