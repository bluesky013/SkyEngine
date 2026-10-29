# Aurora RHI

Aurora 是 SkyEngine 在 `dev_refactor_rhi` 分支上重写的 RHI（取代旧 `engine/rhi`）。
三个后端：Vulkan / DX12 / Metal。
**GLES 不再支持**：接口 `API` 枚举不含 `GLES`，也不提供 `AuroraGL` 后端动态库；
移动端统一走 Vulkan（Android）或 Metal（iOS），不要为 GLES 新增任何接口/后端代码。
后端以独立动态库形式编译（`AuroraVulkan` / `AuroraDX12` / `AuroraMetal`），
通过 `Instance::Init({api = ...})` 在运行时 dlopen 选择。

Aurora 属于 foundation，接口与实现均留在 `engine/aurora`，不得创建 Aurora plugin。Renderer 接口位于 `core/include/aurora/renderer`，实现位于 `core/src/renderer`，测试位于 `core/test/renderer`；实现并入 Aurora.Core.Static，Framework 注册归 Aurora.Adaptor.Static，沿用 Aurora.Render 模块入口。Aurora.Renderer.Interface 仅暴露接口依赖，不建立第二套 core/adaptor/runtime。原生后端保留 `engine/aurora/rhi/{vulkan,dx12,metal}`，`rhi/common` 共享 slot/提交生命周期、有界 buffer 分配和 device transient 池/借用绑定，不统一后端 compiler/executor。目录命名如需调整另行 review，不迁入 plugins。

B11 的输出调度位于 `core/src/renderer/frame/FrameScheduler`，先 CPU prepare/可用 lease，再 timeout=0 acquire/复核，Compile 不获取窗口。输出借用、提交同步与 presentation 退休分别保活，完成只由现有 poll owner 转发；没有新 target 或第二个 ring。B12 已接生产 provider/主循环，B13 沿用单图合成前 acquire。

B14 的 Windows AuroraModule 与 EditorRenderer 均消费 IRenderer；RendererHost 在既有 adaptor 中管理唯一服务与停止事件泵，SceneProxy 只追加 owned 生命周期命令。TrySubmit/PollEvents 归单一宿主 producer，不能让多个模块各用一套 sequence 或分抢事件。编辑器主线程完成所有窗口 UI 布局、字体/图标更新后生成 owned 快照，渲染端只解析精确资源和逻辑 ViewOutput；真实 loader lease 保活 feature 代码。Win32 窗口延迟 WM_CLOSE 的原生销毁，收到退休回执后才释放；拥有窗口的 Application 派生类须在窗口成员析构前调用幂等 Shutdown。Windows 旧 GPU loop 编译排除，非 Windows 保持原整套宿主路径；World 自动提取归独立 scene-bridge。

## 命名空间

接口与所有后端一律使用 **`namespace sky::aurora`**。
不要新增 `aurora::rhi` 或其它别名；这是 review 中确认过的规则。

## 默认值约定（quick-fixes change 设定）

| 项 | 默认 |
|---|---|
| `Sampler::Descriptor::magFilter` / `minFilter` | `Filter::LINEAR` |
| `Sampler::Descriptor::mipmapMode` | `MipFilter::LINEAR` |
| `Sampler::Descriptor::maxLod` | `1000.f`（VK 惯例，避免屏蔽 mipmap） |
| `Sampler::Descriptor::addressModeU/V/W` | `WrapMode::REPEAT` |
| `DeviceFeature::*`（包括 `meshShader`） | `false`；后端在 `UpdateDeviceCaps` 时按实际能力打开 |

## 接口层常量

定义在 `aurora/rhi/Core.h`：

- `MAX_COLOR_ATTACHMENTS = 8`
- `MAX_VERTEX_BINDINGS   = 16`
- `MAX_VIEWPORTS         = 16`

后端 Encoder 在 `BindVertexBuffers` / `SetViewport` / `SetScissor` 中
**用 `SKY_ASSERT` 检查上限，不再 silent clamp**。
超出上限是调用方 bug，不是被静默截断的 feature。

## Device 初始化时序

`Device::Init()` 顺序：

1. `OnInit(devInit)` — 后端创建 native device
2. **`UpdateDeviceCaps()`** — 后端填 `capability.maxThreads` 等

并行编码的 ThreadPool / ThreadContext 目标归 `DeviceFrameContext` 持有，`parallelNum` 只表示 GPU 编码并行度。
当前 Vulkan/DX12 已实现 slot/coordinator command 资源、typed buffer copy 冻结图及各自串行 Compile/Dispatch；Vulkan 双 compiler/encoder 覆盖单 color image upload/raster/readback，并已验证同 attachment 连续 LOAD 的多 subpass、独立 dynamic scopes 和模式对应 inheritance 计划。shader local read 显式拒绝。Vulkan parallelNum>1 已按 slot/实际 worker/ordinal 录制 secondary，TryCollect 在 coordinator 收齐并稳定组装 primary；单 worker 不建 pool，仍走 inline。Vulkan native 故障/取消/teardown 已验证；错误汇总不得掩盖任一 DeviceLost。DX12 已由独立 compiler/encoder 生成单 color image/raster/copy 计划，按实际 worker/slot/ordinal 并行录制 DIRECT lists 并稳定提交；紧密行 copy 的 footprint scratch 随计划退休，Close 失败后丢弃 list 并安全重建。B04 已实现 Vulkan/DX12 的 descriptor/staging/UBO/scratch 显式 lease 分区和容量检查；B07 已补 scalar UBO、indexed/depth raster、ShaderResolver 前置解析及 scope 内 PSO 复用；B10 已补当前纹理/MSAA/resolve/cache 子集，所有平台已切换 RendererHost；Metal 新协议已支持 slot/clear/copy/raster draw、D32 depth、RGBA8 MSAA resolve 与 mip/layer view；compute/custom 已接声明访问、barrier/内容分析与后端编码（Metal 实机待 P02）；iOS/Android 实机验收仍待实施。CPU prepare 使用独立 pool，不从 context 取得 pool/arena。

B05 的 `ResourceCatalog` 位于 Aurora core 的 renderer/resource，只在 coordinator 修改；prepare 只消费冻结 CPU metadata，backing pins 留给 coordinator/submission。`usageVariant` 表示布局兼容族，回退必须成组选择同版本 metadata/backing。`ResourceUploader` 已接通 buffer/RGBA8 2D image 共用分块上传：必须核验全部 native RecordedFrame copy 回执、Accepted 和成功完成，才能经内部 `AdoptResident` 发布。uploader 独占该 context 的完成轮询并透传其它完成；取消和错误不替代 GPU drain。每版本仅一个未决块，Accepted/完成进度分离，完整内容才 InGraph/Resident；部分 backing 单独限额，销毁 owner 不保留可被新 backing 误用的完成前缀。两种资源共享 pending backing 的逻辑字节/数量预算；第十三子批已自动纳管 backing，tracking 租约随 Resident/冻结版本转交，最后 owner 和 GPU 使用退出后由 context 回收；第十四子批已接逐 mip/layer 边界与混合状态续传；其它格式和跨队列仍待后续。

B08 的 PreparedScene 在 Aurora core 的 renderer/prepare 复用 CPU scheduler：每 Scene 融合 Object/Bounds/point-light，每 View 分块只读提取；资源版本变化参与 dirty 判断。只把 CPU metadata/逻辑键放进 PreparedFrame，不把 RHI backing 交给 worker。PreparedRaster 在渲染 coordinator 解析精确版本；失败不得偷偷换 mesh。原固定 View/Material/Object UBO 已由 `fix-aurora-pass-resource-contract` 移出通用层：Pipeline 的 SceneRenderFeature 消费 OpaquePass/ShadowPass 与各自 Batch 声明，宿主显式注册；其它 Pass 可使用不同布局或省略 View。

资源层级保持 **Global（set 0）/ Pass（set 1，View 按需）/ Material（set 2）/ Batch（set 3）**；Material 接口跨 P/V variant 固定，Material 与 Batch 不混放。具体 Pass 维护权威 `.h` 声明源，C++ 直接包含它，shader 包含由声明导出的持久化 `.slangh`；导出位于模块 `assets/shaders/<owner>/...` 并入版本控制。C++ 编译不得依赖 Slang/ShaderHeaderTool 生成镜像；CMake 负责导出检查与统一资源清单部署，实际 shader 反射负责校验布局与资源；不得手写第二套结构/字段表或以固定 80/64 字节、Material=set 1 取代此契约。PreparedRaster/RendererRuntime 消费实际 Pass 声明，不决定所有 shader 的业务布局。不可变布局可共享，可变数据仍遵守 frame lease 与 GPU 退休。

生产场景 Pass 由 `RegisterScenePass` 追加到宿主首个 packet；Adaptor 单向依赖 Pipeline，Core 不引用具体 Pass。缺少/禁用场景 feature 时没有隐式 Raster fallback。FrameGraphBuildContext 的 ResolveDraws 只借用当前 BuildView 快照，完整 catalog/feature owners 由协调器交给冻结图保活；WriteUniform 按显式类型分配并绑定实际 payload 长度，不把对齐 padding 计入可读参数范围。

B09 的 Core Completion/ContinuationExecutor 已接入 prepare：终态只在 kernels 收齐、内部归并/借用清理后发布，用户 continuation 在指定 executor 执行。串行回退为同一 prepare pool 的单任务，仍可取消。CancelRequested 不代表完成，外部 read views/continuation captures 另行阻挡下一 packet 和 CPU 存储释放。RequestShutdown 仍收齐已接受包；主动取消走 RequestCancelPrepare。当前 CPU coordinator 失败/取消后须完整重建，生产模块卸载与 arena/GPU 组合仍待 B12/B13。

B10 已实现 image 重复导入统一、完整 mip/layer copy、descriptor-only transient、device pool/frame lease，以及同图整对象 alias。只将完整描述相同且存活区间严格不重叠的 transient 映射到同一 backing；逻辑内容独立，原生物理同步由各 compiler 生成。跨帧只复用已归还的同描述/epoch backing。第四子批已接通完整 RGBA8 2D image 上传/Resident：载荷按 layer 再 mip 紧密排列，整版本接纳，图内 handle 不等于全局就绪；ResourceBacking 的 buffer/image variant 防止类型混用。第五、六子批已接 Texture2D/Texture2DArray/sampler、类型反射、显式 mip/layer view 与三模式实际采样/双在途保活；只要求 view 暴露的内容已定义。该阶段的 cache v2 已由后续格式替代，当前 shader index/blob v10，旧产品须重新 cook；第七子批已接部分 initializedRanges 导入与物理子资源状态，图初末 layout 仍统一；copy/view 按范围转换，alias 不继承内容，完整 export/Resident 要求不变。第八子批已支持局部矩形与显式 texel 行距，padding 不属于有效内容；第九子批已接 initializedRegions 跨图矩形内容声明；第十子批已接 RGBA8 image 分块、提交/完成进度及有界部分 backing；第十一子批已接 buffer 字节内容/分块，两者共用进度/退休/预算；第十二子批已接同 context/graphics queue 的显式资源状态账本，第十三子批已接自动 uploader 纳管/退休和 accepted 状态导入，第十四子批已接混合 mip/layer 图边界与账本，第十五子批已接 RGBA8/D32 显式单 mip/layer 附件 view，第十六子批已接 RGBA8 2/4/8× color MSAA/resolve，第十七子批已接匹配采样数的 D32 MSAA depth；depth resolve、更多格式/跨队列留后续。原生后端保留原 RHI 目录，目录 change 独立处理。

如果你新增了 capability 字段，在 `UpdateDeviceCaps()` 里写它。

## ResourceGroup 创建

接口为 **`Device::CreateResourceGroup(const ResourceGroup::Descriptor&)`**，`Descriptor` 含 `{Shader *shader, uint32_t set}`。ResourceGroup 的 descriptor 布局从 **shader reflection** 派生，没有独立的 `ResourceGroupLayout` 对象（`aurora-remove-resource-group-layout` change 已移除）。

- `shader` MUST 非空；shader 的 `reflection` MUST 非 null（空 reflection 合法）。
- ResourceGroup 复用 shader 内派生的 native set layout（Vulkan `GetDescriptorSetLayout(set)` / DX12 从 reflection 算 descriptor 数量）。
- binding 类型统一用 `ShaderResourceType`（含 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC`，batch tier 用）；`DescriptorType` / `DescriptorBindingFlags` 已删除。

## 参数更新与原生 descriptor 写入（2026-10-27）

`ResourceUpdateInfo`、`ResourceGroup::Update(vector)` 和带 context 的 `CreateEncoder(context)` 已移除。命名事务与原生数字 binding 写入使用独立接口，不能在同一 encoder 上按模式切换：

- `ResourceGroup::CreateParameterUpdate(context)` 返回 `ShaderParameterUpdate`：Name/typed-handle `WriteData`、带 CPU schema 的 `WriteBlock`、命名 `WriteBuffer/Image/Sampler`；`End()` 成功后 `GetResult()` 发布新的不可变 ResourceGroup。`GetError/Cancel` 归此事务；失败保留旧版本，部分更新继承旧值。上下文显式提供 device、有效 lease/epoch、uniform 分配和 retention。
- `ResourceGroup::CreateEncoder()` 只返回 numeric `DescriptorEncoder`：`WriteBuffer(binding, buffer, offset, range, arrayElement)`、`WriteImage`、`WriteSampler` 与 `End()`。调用方保证 group 可写且旧 GPU 使用已退休；已发布 parameter snapshot 拒绝创建 writer。错误写入使 `End()` 失败，低层写入不承诺跨 group 或原地修改的原子性。
- 两条 materializer 共用 `ShaderParameterTransaction`、`BindingPlan` 与 `UniformUploadPlan` 的适用部分；命名解析、语义类型和字段校验不在各后端另写一套。数字 writer 不再保留 Name 占位方法、`GetResult` 或命名取消接口。
- **Vulkan**：数字 writer 按 Shader 的 native set plan 校验后提交 descriptors；保留合法 update-template/显式更新路径。dynamic binding 的 range 必须非零并满足范围/对齐，失败显式返回，不能 assert/warning 后继续。
- **DX12**：按 BindingPlan 区分 CBV/SRV/UAV 与动态 root 参数。旧 direct 路径保留 CPU staging 和 shader-visible ring，只有独立 `DirectFrameContext::TryBeginFrame` 成功后选择退休 slot；生产图改由当前 lease 的 descriptor heap 分区冻结，不能消费旧 ring。
- **Metal**：按 Shader 派生的 stage/category/index 保存并绑定资源，不能从 logical set 或 `binding + arrayElement` 猜物理槽位；未支持的 descriptor array/model 显式拒绝。dynamic offsets 按实际 set/binding 顺序应用。

历史记录：`aurora-frame-descriptor-batch` 于 2026-09-14 增加过跨 set 的 `DescriptorBatch`；2026-10-27 的 `refactor-aurora-framegraph-contracts` 确认它只有测试调用，已移除接口、Device 工厂和三个后端实现。跨 set 原生写入使用各 group 的 numeric writer，生产图使用后端按 lease 管理的 materializer，不恢复第二套 batch/Flush/Reset 协议。

## Frame context 的两种公开协议（2026-10-27）

- `Device::CreateFrameContext(info)` 返回只公开 lease 协议的 `DeviceFrameContext`：取 slot、分配、Compile、Dispatch/收齐、Submit、丢弃与完成查询。生产 renderer 的全部 viewport 共用此 context。
- `Device::CreateDirectFrameContext(info)` 显式创建独立 `DirectFrameContext` compatibility 接口，只有 `TryBeginFrame/EndFrame/GetFrameFence`。后端可复用内部 slot storage，但创建时固定模式；getter 不选择协议、不推进 slot。
- direct begin 非阻塞检查 `Fence::Query`，PENDING 返回 Busy 且不 reset；SIGNALED 后检查 `TryReset`，native 错误显式返回。begin/end 必须配对，调用方负责直接提交及销毁前的 GPU 退休。不能通过 direct 接口访问图分配/状态账本，也不能把其 fence reset 规则带回 lease 取槽路径。

## Dynamic UBO pack（batch tier / set 3）

Batch set 3 的数据由当前 DeviceFrameLease 保活。只有 `Shader::Descriptor::bindingPolicies` 显式选择时才使用 dynamic UBO；不得直接修改目标反射来切换类型。

- CPU 参数通过实际 shader 的 `UniformUploadPlan` 编码，再用 `DeviceFrameContext::WriteFrameBuffer` 分配；目标逻辑 range 与分配 padding 分开。
- dynamic binding 保留 base offset、range 与 dynamicOffset，按真实 set/binding 编码；零 range、越界、对齐错误显式失败。
- backend compiler 在 Compile 冻结 descriptors，worker 不修改；GPU 完成或安全 teardown 前不可复用 backing。不得恢复独立 Reset 的旧 batch allocator。

## Vulkan dynamic rendering 与 stencil

`VulkanGraphicsEncoder::BeginRendering` 处理 depth + stencil：
- format 含 depth 时挂 `pDepthAttachment`
- format 含 stencil（D24_S8 / D32_S8）时挂 `pStencilAttachment`，
  load/store op 来自 `DepthStencilAttachment::stencilLoadOp/stencilStoreOp`
- `clearValue` 同时被 depth 与 stencil attachment 共享

format 的 hasDepth/hasStencil 通过 `GetImageFormatInfo(pixelFormat)` 查询。

## 平台/特性下限

- **Vulkan**：1.1 起步，真实 `timelineSemaphore`（core 1.2 / KHR）和 `synchronization2`（core 1.3 / KHR）强制；支持的 render-pass2 或 dynamic rendering 路径必须可用。按 physical-device API 与扩展查询、启用能力并选择对应 core/KHR 命令，不能把 loader 版本当作设备版本。FrameContext 默认 AUTO，优先 dynamic，否则使用已有 RenderPass compiler；显式模式不支持时失败。直接 graphics encoder/PSO 工厂仍要求 dynamic rendering。shader 产品须满足对应目标环境契约；PSO 从 `PipelineState::vertexBindings/vertexAttributes` 建 IA 输入布局（与 DX12 对齐）；tier2 `DescriptorHeap`（`VK_EXT_descriptor_heap`）**未实现**，`CreateDescriptorHeap` 返回 nullptr 且 `GetFeature().descriptorHeap == false`
- **DX12**：12.0 起步；PSO / root signature / ResourceGroup / SwapChain 均已落地，encoder 支持 render target 绑定与 clear、IA 顶点输入、indirect draw/dispatch、内置 fullscreen blit 与 tier2 bindless `DescriptorHeap`（SM6.6 门），见 `aurora-dx12-gaps`
- **Metal**：3 起步；PSO 从 `vertexBindings/vertexAttributes` 建 `MTLVertexDescriptor`；**buffer 槽位分区**——Metal 每 stage 31 个 `[[buffer(N)]]` 槽（0..30），shader buffer 绑定按目标反射占低段 `[0, 15)`，顶点缓冲占高段 `[15, 31)`（`METAL_VERTEX_BUFFER_SLOT_BASE = 31 - MAX_VERTEX_BINDINGS`，`BindVertexBuffers` 与 vertex descriptor 一致使用）；shader buffer 地址越界须在计划/创建阶段显式失败。当前 target-artifact DirectArguments 尚不支持原生 push-constant slot 映射，必须拒绝，不能按 declare-last 猜槽位；保留的 partial-range staging helper 不代表生产路径支持。Blit：同格式同尺寸走 blit encoder 原生 copy，缩放/过滤走 `MetalBlitHelper` 的 fullscreen-triangle render pass（dst 需 RENDER_TARGET usage）；直接 RHI 的 Blit Resolve 保留空 render-pass helper；生产 FrameGraph attachment resolve 必须在原 raster pass 内按 STORE/DONT_CARE 选择 store-and-resolve/resolve，不得先丢弃 source 再额外 LOAD。Blit helper 由 `MetalBlitEncoder` 挂起/恢复 blit encoder 包住。SwapChain：3-image ring（`maximumDrawableCount=3`），`framebufferOnly=NO`（backbuffer 可采样）；acquire 仍是 CPU 立即 signal（Metal 无 GPU acquire 语义）。压缩格式：BC1-7（桌面 GPU）+ ASTC（Apple GPU）已映射，ETC2 无 Metal 对应。tier2 `DescriptorHeap` **未实现**：`DescriptorHeap` 接口按 `VK_EXT_descriptor_heap` 设计为**索引型堆**（tex/buf/smp 各自 backing buffer + per-type stride），而 slang 在 Metal 上走的是**指针/地址型 bindless**（`DescriptorHandle<T>` → `device T*` / `ulong`，无 argument buffer 的 `[[id]]` 形态）——两者模型不同。slang 对 Metal bindless `DescriptorHandle<T>` 的 **compiler emit 已完成**（slang #10842；本仓库 pin 的 v2026.9.2 已含），缺口在 (a) 我们未采用 `DescriptorHandle<T>` 的 shader/RHI 模型、(b) Metal 运行时实现、(c) 纯 `uniform` 数组-of-resource 仍有 bug（slang #12291，Q3 2026）。故 `feature.descriptorHeap == false`、`CreateDescriptorHeap()` 返回 nullptr。落地路线见 `openspec/changes/aurora-metal-bindless-descriptor-heap/`

## Metal 后端实现约定（ARC / 句柄 / 线程池 / MSL 槽位）

- **ARC**：`Aurora.Metal` 的 `.mm` 单独开启 `-fobjc-arc`（`metal/CMakeLists.txt` 对 `src/*.mm` 设 `set_source_files_properties(... "-fobjc-arc")`）。**不得**混用手工 `retain/release/dealloc`（ARC 下编译报错）。不要给整个后端加 `-fobjc-arc` 到 `.cpp`（普通 `.cpp` 不处理 Obj-C）。
- **native 句柄**：Obj-C 类型不进头文件，句柄一律 `void*`。跨越 `id` ↔ `void*` 必须用 bridge cast：存句柄 `(__bridge_retained void *)obj`（+1，脱离 ARC），读句柄 `(__bridge id<MTLXxx>)handle`（借用），释放 `(void)(__bridge_transfer id<MTLXxx>)handle`（+1 交还 ARC）。**每个 `__bridge_retained` 必须恰有一个配对的 `__bridge_transfer`**（否则泄漏；MRC 下这些 cast 是 no-op，正是历史泄漏根因）。
- **Device 工厂**：统一走 `MetalDevice.mm` 内部模板 `CreateAndInit<T>(device, desc)`（`new T` + `Init` + 失败 `delete`），不逐方法复制样板。
- **MSL 槽位**：`src/MetalMSLSlots.h` 的 `BuildMetalMSLSlotLayout(reflection, set)` 是**唯一**来源——slang MSL 按类别（buffer/texture/sampler）各自从 0 顺序编号，忽略 `[[vk::binding]]`；`MetalShader`（push constant 槽 + buffer 预算）与 `MetalResourceGroup`（类别绑定槽映射）共用它。
- **线程池 / autorelease pool**：主线程帧池在 `MacosPlatform::PollEvent`（帧首建、次帧首 drain）；worker 线程池在 `MetalThreadContext::OnAttach/OnDetach` 用 `objc_autoreleasePoolPush/Pop`（ARC 禁 `NSAutoreleasePool`）。**ARC 不自动建池**；任何创建 autoreleased 对象的线程都必须有池。需跨帧/等 GPU 的对象**不可**依赖 autorelease pool（池 drain 早于 GPU 完成），必须显式强持有（`__bridge_retained` 句柄或 Metal 内部持有），GPU 完成信号用 `encodeSignalEvent` + fence（见 `MetalQueue::PendingUpload`）。

## Queue / Submit / Semaphore / SwapChain（submit-present）

提交与呈现路径（`aurora-queue-submit-present` + `aurora-client-viewport`）。

### Queue::Submit 契约

- 新 Renderer 接入使用 `Queue::TrySubmit`：Accepted 仅表示接受；Rejected 保证无入队；Indeterminate 必须保留引用并隔离到安全完成/teardown。Vulkan/DX12 已实现，Metal 新入口当前明确 Unsupported。
- 新路径使用 `Fence::Query` 区分 Pending/Signaled/Failed、`TryReset` 读取 reset 错误。Reset 不产生 submission；拒绝后的 Pending 不能作为等待理由。旧 direct 宿主现在显式使用 DirectFrameContext compatibility；成功 begin 后仍须配对直接提交，不允许空转复用未提交的 fence，lease 取槽不执行此 reset。

- `Queue::Submit(SubmitInfo)`：`SubmitInfo` 含 `commandBuffers` / `waitSemaphores` / `signalSemaphores` / `fence`。
- `SemaphoreSubmitInfo::stageMask` 表达 wait 前 / signal 后的 pipeline stage（sync2 语义）。

### Semaphore binary vs timeline

- `SemaphoreType::BINARY`：跨 submit 的 GPU-GPU 信号（swapchain acquire → render-done → present）；值由后端隐式 +1，调用方不传 value。
- `SemaphoreType::TIMELINE`：host 与 GPU 都可 signal/wait，调用方显式传 value；跨队列同步用它。

### SwapChain 契约

- `AcquireNextImage(signalSema, fence, timeoutNs) -> index`：拿下一帧可渲染 image，signal binary sema（+ 可选 fence）。
- `Present(imageIndex, numWaitSemas, waitSemas)`：等待 render-done sema 后把 image 提交给窗口系统。
- `Resize(w, h)`：重建全部 image；旧 `GetImage` 指针全部失效。
- `GetStatus()` / `GetSurfaceSize()`：surface 尺寸由原生窗口决定，RHI 层自查；`OUT_OF_DATE` 触发 Resize，`SUBOPTIMAL` 当 `OK`，`LOST` 取消当帧 present。

### 后端差异

| 后端 | binary sema | timeline sema | Present | stageMask |
|---|---|---|---|---|
| Vulkan | `vkQueueSubmit2` binary sema | `vkSignalSemaphore` / `vkWaitSemaphores` | `vkQueuePresentKHR`（Submit 后） | 原生支持 |
| DX12 | `ID3D12Fence`（queue 级 `Wait`/`Signal`） | 同左，value 显式 | `IDXGISwapChain3::Present`（CPU wait render-done fence） | **忽略**（DX12 fence 是 queue 级，不能按 stage 阻塞） |
| Metal | `MTLSharedEvent` + 内部 binary counter | `MTLSharedEvent` + 显式 value | 每次 Present 起 fresh cmdbuf 挂 `encodeWaitForEvent` + `presentDrawable`（不与用户 Submit 链耦合） | 忽略 |

### Viewport / present 编排

- `RenderViewport`（`aurora/rdg/RenderViewport.h`）是表面基类：`Begin`/`Acquire`/`Release` + backbuffer/format/extent + acquire/render-done sema 访问器（不含 frame index/fence）。
- `ClientViewport` 持 `SwapChain` + per-viewport 帧 ring sema（本地 `mFrameSlot` 计数，非 image index、非全局 frame index）。
- inflight frame 全局化：`DeviceFrameContext` 是 slot ring 的唯一所有者；新路径经 lease、Submit/PollCompleted 驱动。direct 宿主通过独立工厂取得 DirectFrameContext；lease 接口不再暴露 Begin/End/GetFrameFence，多 viewport 共用同一 lease。
- 输出导入：FrameScheduler 取得有效输出 lease 后由 FrameComposer 导入图；Compile 不 acquire/present，borrowed swapchain image 不构造拥有删除权的 CounterPtr。

## Barrier 用法

`PipelineBarrier` 在 **`CommandBuffer`** 上，**不**在 Encoder 上。可在以下任意时机调：
- Encoder 创建之前（典型：pass 之间 transition）
- Compute 序列中两次 Dispatch 之间
- BlitEncoder 内部之前 / 之后

### AccessFlags → ImageLayout 推导

`AccessFlagBit` 是 **stage-agnostic 的访问类别**（`SRV` / `UAV` / `CBV` / `RTV` / `DSV` / `DSV_READ` / `COPY_SRC` / `COPY_DST` / `PRESENT` / 顶点输入 / `GENERAL`），stage 由 `BarrierInfo.srcStage/dstStage` 单独表达。

`InferLayoutForAccess(AccessFlags)` 提供从 access 类别推导 canonical layout 的便利函数：

| access | layout |
|---|---|
| `RTV` | `COLOR_ATTACHMENT` |
| `DSV` | `DEPTH_STENCIL_ATTACHMENT` |
| `DSV_READ` | `DEPTH_STENCIL_READ_ONLY` |
| `SRV` | `SHADER_READ_ONLY` |
| `UAV` | `GENERAL` |
| `COPY_SRC` | `TRANSFER_SRC` |
| `COPY_DST` | `TRANSFER_DST` |
| `PRESENT` | `PRESENT` |
| 多类冲突 / 无 layout 概念 | `GENERAL` |
| `NONE` | `UNDEFINED` |

### Stage / Access 兼容性陷阱

Vulkan validation 严格检查 stage ↔ access。stage 由调用方显式填 `BarrierInfo.srcStage/dstStage`；RDG 按 pass 类型推断。常用配对：

| access | stage |
|---|---|
| `RTV` | `COLOR_OUTPUT` |
| `DSV` / `DSV_READ` | `EARLY_FRAGMENT \| LATE_FRAGMENT` |
| `SRV` / `UAV` / `CBV`（raster pass） | `VERTEX_SHADER \| FRAGMENT_SHADER` |
| `SRV` / `UAV` / `CBV`（compute pass） | `COMPUTE_SHADER` |
| `COPY_SRC` / `COPY_DST` | `TRANSFER` |
| `PRESENT` | `BOTTOM` |

### 4 后端实现位置

| 后端 | 路径 |
|---|---|
| Vulkan | `vkCmdPipelineBarrier2` 直接落 cmdbuf |
| DX12 | `ID3D12GraphicsCommandList::ResourceBarrier` 直接落 cmdlist；UAV barrier 用 null pResource |
| Metal | `MetalCommandBuffer` 内部记账 active encoder：调用时若有 → `[encoder memoryBarrierWithScope:]`；若无 → 缓存到下一次 CreateXxxEncoder 入口 flush。Layout transition 在 Metal 上 noop。Blit encoder 不暴露 memoryBarrier API（依赖 encoder 边界隐式同步） |

## FrameGraph 用法

生产图由 `core/src/renderer/graph/FrameComposer` 与 `FrameGraphBuilder` 构建，通过 `FrameGraphBuildContext` 调用已注册 feature，Freeze 后成为不可变 `FrozenFrameGraph`。公共 IR 与纯分析留在 `rhi/interface`。

1. 声明资源、typed copy/raster/compute/custom passes 和 exports；handle 包含 graph identity，跨图或越界返回显式错误。
2. 使用有效 DeviceFrameLease 调用活动后端 `DeviceFrameContext::Compile`，校验内容/依赖并生成 native plan、barriers 与冻结 bindings。
3. Dispatch/收齐编码结果，再显式 Submit；GPU、CPU borrow 与 presentation 按各自退休条件保活。

Vulkan 的两套 compiler、DX12 frame compiler 与 Metal frame context 保留各自 native 编译策略；共享 `FrameTransientPool` 只管理 device backing/lease，不统一 encoder。Custom 仅访问声明资源，不自行插入 barrier。

旧 RenderGraph/CompiledGraph、RDGBackend/CreateRDGBackend 和 PipelinePass 模板已退役，不新增兼容注册入口。直接 RHI encoder、RenderViewport/ClientViewport 仍可用于显式创建的 DirectFrameContext compatibility 宿主；生产 lease context 不暴露 direct 方法。共享 Pass shader 声明与 SceneRenderFeature/TextureToScreenFeature 保留。

## 冻结绑定组与编译访问事实（2026-10-27）

- `RasterDraw` / `FrameComputePass` 只持 `FrameBindingGroups`，不保留 flat bindings 的永久副本。`FrameBindingGroup` 的 const data 保活 layoutOwner，保存真实 set、graph handles、base range/view 与 sampler；构造时按 binding 规范化。
- `FrameBindingGroupUse` 的 `bufferUses/imageUses` 单独保存 per-use access 和 dynamic offsets。相同 shader/layout owner 与资源值可共享组；动态窗口变化不使其它组重建。不同 graph resource/version、base range/view 或 layout owner 不得误命中。
- 追加绑定先构造临时 `RasterBindingDraft/ComputeBindingDraft`，经 `SetRasterBindings/SetComputeBindings` 原子发布；非法 set/binding 或重复槽拒绝并保留旧组。`ValidateBindingGroups` 必须在读取前拒绝 unknown/duplicate overrides、owner/set 不符或无 snapshot 的 overrides。
- `ResolveRasterBindings/ResolveComputeBindings` 只提供 Compile 内临时 flat values；Custom 保留有序声明数组和回调资源顺序。不能为统一模型持久化第二份规范化 IR。
- `FrameGraphAnalysis::passResourceUses` 复用同一 typed 声明遍历，保留有效 byte/subresource 范围、usage role、stage 和 access；内容 reads/writes/defines 独立。结构/reflection 在 culling 前验证，只有存活访问与 exports 要求内容已定义。
- 各 native compiler 从上述访问事实生成自己的同步；同一物理 buffer 的 vertex/index/uniform/storage 同时只读角色先合并，不能漏掉 raster storage 或以最后一个角色覆盖前一个。Vulkan 两种 compiler 仍各自生成 scopes/encoder。
- `FrameGraphDeclarations` 只提供 transient buffer/image、copy、raster/fullscreen、compute、Custom、exports；`FrameGraphServices` 提供资源/几何解析和 upload/factory。`FrameGraphBuildContext` 组合这两个 borrowed facets，不暴露 Submit。
- 图参数先经共享语义事务 Complete，再由 `FrameGraphServices::MaterializeParameters` 取得 immutable group。`FrameComposer` 在当前 composition/lease 内按 shader/set、typed fields 和精确资源值缓存目标 ABI 编码与 uniform upload；native materializer 按组复用 descriptor，worker 不写参数或 cache，GPU pins 仍按既有 lease 退休。

## 骨架 / 蒙皮（animation 独立，aurora 无 Skeleton）

`Skeleton` 属于独立模块 `engine/animation`（target `Animation`，只依赖 `Core`），**aurora 不定义也不依赖它**。旧引擎同理：`render/core` 自持 `Skin`，只有 `render/adaptor` 才 link `Animation`。

| 类型 | 位置 | 含义 |
|---|---|---|
| `Skin` | `aurora/resource/Skin.h` | 独立只读绑定资源：输入槽契约、inverse bind、Named/Indexed 骨目标；不含姿势 |
| `SkinnedMesh`（scene 组件） | `aurora/scene/SceneTypes.h` | 实例：持 `CounterPtr<Skin>` |

约定：

- **不要在 aurora 里定义 `Skeleton` / `Bone`**，也不要新增 `aurora/animation` 之类目录；`aurora/core` 不依赖 `Animation`。
- `animation::Skeleton`（+ pose）→ Aurora 的目标布局与模型矩阵转换放在**桥接层**，由桥接层 link `Animation`。`ResolvedSkinBinding` 按精确资源版本解析；调色板归实例/帧，不写回 Skin。
- 渲染侧每帧求值若需独立类型，叫 `SkinningPalette` / `SkinMatrices`，不要占用 `Skin`。
- 按已批准 `add-skeletal-animation-workflow`，Mesh 不持 Skin 或 Skeleton；实例显式选择独立 Mesh / Skin / Skeleton。禁止恢复 `Mesh::SetSkin/GetSkin/HasSkin`。

## 后续 change 路线

| Change | 状态 | 说明 |
|---|---|---|
| `aurora-quick-fixes` | ✅ 已实施 | 本文档所述默认值/命名 |
| `aurora-queue-submit-present` | ✅ 已实施 | Queue / Submit / SwapChain Present（3 后端） |
| `aurora-renderpass` | ✅ 已实施 | `FullScreenPass` 基类 + `ScenePass`(HDR) + `TextureToScreenPass`（renderpass begin/end 契约） |
| `aurora-client-viewport` | ✅ 已实施 | `RenderViewport` / `ClientViewport`；生产输出现由 scheduler lease 导入图 |
| `aurora-encoder-barriers` | ✅ 已实施 | `CommandBuffer::PipelineBarrier`（最终落在 cmdbuf 而非 encoder） |
| `aurora-resource-group` | ✅ 已实施 | ResourceGroup / 描述符绑定（Vulkan + DX12 + Metal 直接绑定，dynamic offset 三后端齐） |
| `aurora-remove-resource-group-layout` | ✅ 已实施 | 移除 ResourceGroupLayout，ResourceGroup 从 shader reflection 派生 |
| `aurora-renderer` | Windows 主流程与宿主已接入 | Vulkan 双 compiler / DX12；Metal 对等仍待 P02 |
| `aurora-rdg` | ✅ 已实施 | render graph（三段式 RDG） |
| `aurora-dynamic-ubo-pack` | ✅ 已实施 | dynamic UBO stable binding 保留；旧 allocator/writer 已由 FrameContext lease 写入替代 |
| `aurora-launcher-integration` | ✅ 已实施（最小闭环） | `AuroraRender` 动态模块（`IModule` + `REGISTER_MODULE`）+ 窗口/swapchain/clear/present 闭环；launcher 只加载 aurora，移除旧 `SkyRender`/`RenderAdaptor`；引擎内置 `configs/` |
| `aurora-adaptor` | ✅ 已实施 | `engine/aurora/adaptor` 双 target：`AuroraRender`(SHARED) + `Aurora.Adaptor`(STATIC；`AuroraReflection` + assets + components)，对接 framework `SerializationContext`/`AssetManager`/`ComponentFactory` |

详见 `openspec/changes/<name>/`。

## Windows 宿主与模块生命周期

- Aurora 属于 foundation，继续使用 `Aurora.Adaptor.Static`、`Aurora.Render` 和 `Aurora.Render.Editor`，不新增 plugin 或重复 renderer 子模块。
- Windows AuroraModule、EditorRenderer 均经 RendererHost 创建/发布唯一 `Interface<IRenderer>`。各宿主只有一个 packet producer/事件消费者；其他模块不能独立分配 sequence、提交或消费事件。SceneSource/RenderSceneProxy 只在主线程追加 owned 命令，World 提取归 scene-bridge。
- EditorApplication 在模块加载前选择 provider-only；SandboxModule 显式依赖 Aurora.Render、shader compiler 和编辑器扩展，provider 先就绪、消费者先退出。编辑器主/工具窗口与离屏图共用一个 Renderer/Device/FrameContext。
- UI 布局、字形、图标、输入和窗口操作留主线程。UiSnapshotProducer 只持 CPU 数据，全部窗口完成图集修改后才生成精确版本快照；UI texture 0 是白图，1 是场景离屏结果，其余为字体/图标。GPU 上传、SDF/采样、clip/alpha、图像依赖和 native 缓存由渲染端 feature 处理。
- UI 数据与资源沿用现有有界资源上传和 slot allocator。旧 atlas 版本通过 releasedResources 释放，排队候选、未决 upload 和提交 pins 在安全退出前继续保活，禁止在主线程创建 GPU 纹理或另建上传 ring。
- NativeWindow deferred-close 保护 HWND：工具窗等 ViewportDestroyed 后销毁；整个宿主等 RendererStopped 后销毁 Renderer，再解除窗口保护。Game/Editor/XR 在窗口成员析构前 Shutdown，并在 drain 中保持平台事件泵。
- feature 的 moduleLease 必须来自 ModuleManager::RetainModule，不能用 dummy owner 代替真实动态库。feature 对象先释放，代码 lease 后释放；loader 最后 lease 才 detach/unload，依赖库随消费者代码保活。静态模块代码常驻宿主，复用相同接口。
- 模块 Init 失败先 Shutdown/销毁，依赖消费者不初始化，Application 返回失败。跨 DLL 共享服务使用 Singleton；metadata 由唯一 definition owner 注册，runtime 服务在 Init/Shutdown 对称绑定。
- `AuroraModule` 在所有平台经 RendererHost 创建/驱动唯一 Renderer；旧 non-Windows GPU loop 已删除。Windows 旧 EditorRenderer 编译排除；Metal 新编译/输出/宿主已通过 Cocoa native host test，iOS/Android 实机验收仍待 P02。

## Image 边界状态

- ImageBoundaryState 的显式表必须完整覆盖 layer*mipLevels+mip，统一 layout 与全等数组语义相同；final 不接受 UNDEFINED，已有 texel 不能位于 initial UNDEFINED 的 cell。状态布局不扩大内容资格。
- FrameContext 从 Image::GetMipLayerCount 查询已确认归属的实际 backing cell 数；默认 trackedImageSubresources=65,536，和 backing 数分别限额，owner/GPU 安全退休才归还。混合快照有复制成本，仍由 coordinator 构建；跨队列、aspects/planes 仍未开放；compute/custom 已接声明访问与后端编码（Metal 实机待 P02）；Metal depth resolve 已接。

## Raster attachment view

- RasterAttachmentView 独立于采样 view，color/depth 各选一个 mip/layer，默认 0/0；LOAD/STORE/CLEAR 和内容依赖只影响该 cell，scissor 与 color/depth 匹配按所选 mip extent，整图 export 不扩大资格。
- Vulkan/DX12 的 native view/RTV/DSV 在 Compile 构造并随计划/提交退休，workers 不创建；RenderPass 合并同时比较逻辑附件和两个 view。当前仅 2D；RGBA8 可用设备支持的 2/4/8× color MSAA/resolve，D32 支持匹配 color 采样数的单 sample 或 2/4/8× MSAA；同 pass attachment feedback 仍整 image 拒绝。

- B10 第十六子批的 colorResolve 是 raster 内部独立写入；源 CLEAR/LOAD 保证全样本有效，源丢弃不影响 resolve 结果，目标可选非零 mip/layer。MSAA storage 限 GPU_ONLY/单 mip；RGBA8 仅 RENDER_TARGET，D32 仅 DEPTH_STENCIL，分别预检设备能力且 color/depth extent 与 samples 匹配；不开放 depth resolve、采样、buffer copy 或局部初始化。上传回执把 resolve target 计为额外写入。

场景材质使用 version 4 的显式 program.vertex/fragment 与生成 schema 的 canonical typed properties（旧 aliases 显式迁移），按 queue Tag 选择；不推导入口、不选首个 technique。Global 只承载跨 View 的帧数据，相机归 Pass。Material 占 set 2，逐 draw 的 SceneSurface、palette 和 UI 纹理归 Batch set 3。shader index/blob 当前为 v10，保留三域归属、Vertex 语义条件与 Pipeline 预算；旧缓存须重新 cook。无源码运行不得回退到空 Pipeline 或猜测 SKINNED 定义。


变体补充（2026-10-25）：Core 统一归一化 Vertex / Pipeline / Shader 三域；Material、Batch、fullscreen、compute 是 S 的不同提供者。
业务 API 只传 bool/int32/uint32 Key/Value，JSON 关联 source symbol；compile/native lowering 与 native ID 由对应 target 的源码声明反射决定。
完整选择、stage code、executable 身份分离，native ID 按 stage 独立；产品必须显式发布完整选择。Group 2 零或一个 cbuffer，已知 inactive 值保留并验证类型。
Scene Prepare 在 graph 声明前完成附加程序准备；Scene Pipeline 的四个 SHADOWS/IBL 请求是显式策略，不自动遍历所有选项组合。
