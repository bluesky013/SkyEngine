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
当前 Vulkan/DX12 已实现 slot/coordinator command 资源、typed buffer copy 冻结图及各自串行 Compile/Dispatch；Vulkan 双 compiler/encoder 覆盖单 color image upload/raster/readback，并已验证同 attachment 连续 LOAD 的多 subpass、独立 dynamic scopes 和模式对应 inheritance 计划。shader local read 显式拒绝。Vulkan parallelNum>1 已按 slot/实际 worker/ordinal 录制 secondary，TryCollect 在 coordinator 收齐并稳定组装 primary；单 worker 不建 pool，仍走 inline。Vulkan native 故障/取消/teardown 已验证；错误汇总不得掩盖任一 DeviceLost。DX12 已由独立 compiler/encoder 生成单 color image/raster/copy 计划，按实际 worker/slot/ordinal 并行录制 DIRECT lists 并稳定提交；紧密行 copy 的 footprint scratch 随计划退休，Close 失败后丢弃 list 并安全重建。B04 已实现 Vulkan/DX12 的 descriptor/staging/UBO/scratch 显式 lease 分区和容量检查；B07 已补 scalar UBO、indexed/depth raster、ShaderResolver 前置解析及 scope 内 PSO 复用；B10 已补当前纹理/MSAA/resolve/cache 子集，B14 已切换 Windows 宿主；更多 bindings/compute 与 Metal 新协议仍待实施。CPU prepare 使用独立 pool，不从 context 取得 pool/arena。

B05 的 `ResourceCatalog` 位于 Aurora core 的 renderer/resource，只在 coordinator 修改；prepare 只消费冻结 CPU metadata，backing pins 留给 coordinator/submission。`usageVariant` 表示布局兼容族，回退必须成组选择同版本 metadata/backing。`ResourceUploader` 已接通 buffer/RGBA8 2D image 共用分块上传：必须核验全部 native RecordedFrame copy 回执、Accepted 和成功完成，才能经内部 `AdoptResident` 发布。uploader 独占该 context 的完成轮询并透传其它完成；取消和错误不替代 GPU drain。每版本仅一个未决块，Accepted/完成进度分离，完整内容才 InGraph/Resident；部分 backing 单独限额，销毁 owner 不保留可被新 backing 误用的完成前缀。两种资源共享 pending backing 的逻辑字节/数量预算；第十三子批已自动纳管 backing，tracking 租约随 Resident/冻结版本转交，最后 owner 和 GPU 使用退出后由 context 回收；第十四子批已接逐 mip/layer 边界与混合状态续传；其它格式和跨队列仍待后续。

B08 的 PreparedScene 在 Aurora core 的 renderer/prepare 复用 CPU scheduler：每 Scene 融合 Object/Bounds/point-light，每 View 分块只读提取；资源版本变化参与 dirty 判断。只把 CPU metadata/逻辑键放进 PreparedFrame，不把 RHI backing 交给 worker。PreparedRaster 在渲染 coordinator 解析精确版本并写同一 lease 的 View/Material/Object UBO；失败不得偷偷换 mesh。当前为单 subMesh/距离 LOD/点光源 CPU 数据，生产 provider、完整 pipeline/光照仍待后续批次。

B09 的 Core Completion/ContinuationExecutor 已接入 prepare：终态只在 kernels 收齐、内部归并/借用清理后发布，用户 continuation 在指定 executor 执行。串行回退为同一 prepare pool 的单任务，仍可取消。CancelRequested 不代表完成，外部 read views/continuation captures 另行阻挡下一 packet 和 CPU 存储释放。RequestShutdown 仍收齐已接受包；主动取消走 RequestCancelPrepare。当前 CPU coordinator 失败/取消后须完整重建，生产模块卸载与 arena/GPU 组合仍待 B12/B13。

B10 已实现 image 重复导入统一、完整 mip/layer copy、descriptor-only transient、device pool/frame lease，以及同图整对象 alias。只将完整描述相同且存活区间严格不重叠的 transient 映射到同一 backing；逻辑内容独立，原生物理同步由各 compiler 生成。跨帧只复用已归还的同描述/epoch backing。第四子批已接通完整 RGBA8 2D image 上传/Resident：载荷按 layer 再 mip 紧密排列，整版本接纳，图内 handle 不等于全局就绪；ResourceBacking 的 buffer/image variant 防止类型混用。第五、六子批已接 Texture2D/Texture2DArray/sampler、类型反射、显式 mip/layer view 与三模式实际采样/双在途保活；只要求 view 暴露的内容已定义。cache v2 拒绝旧 blob，离线缓存须重建；第七子批已接部分 initializedRanges 导入与物理子资源状态，图初末 layout 仍统一；copy/view 按范围转换，alias 不继承内容，完整 export/Resident 要求不变。第八子批已支持局部矩形与显式 texel 行距，padding 不属于有效内容；第九子批已接 initializedRegions 跨图矩形内容声明；第十子批已接 RGBA8 image 分块、提交/完成进度及有界部分 backing；第十一子批已接 buffer 字节内容/分块，两者共用进度/退休/预算；第十二子批已接同 context/graphics queue 的显式资源状态账本，第十三子批已接自动 uploader 纳管/退休和 accepted 状态导入，第十四子批已接混合 mip/layer 图边界与账本，第十五子批已接 RGBA8/D32 显式单 mip/layer 附件 view，第十六子批已接 RGBA8 2/4/8× color MSAA/resolve，第十七子批已接匹配采样数的 D32 MSAA depth；depth resolve、更多格式/跨队列留后续。原生后端保留原 RHI 目录，目录 change 独立处理。

如果你新增了 capability 字段，在 `UpdateDeviceCaps()` 里写它。

## ResourceGroup 创建

接口为 **`Device::CreateResourceGroup(const ResourceGroup::Descriptor&)`**，`Descriptor` 含 `{Shader *shader, uint32_t set}`。ResourceGroup 的 descriptor 布局从 **shader reflection** 派生，没有独立的 `ResourceGroupLayout` 对象（`aurora-remove-resource-group-layout` change 已移除）。

- `shader` MUST 非空；shader 的 `reflection` MUST 非 null（空 reflection 合法）。
- ResourceGroup 复用 shader 内派生的 native set layout（Vulkan `GetDescriptorSetLayout(set)` / DX12 从 reflection 算 descriptor 数量）。
- binding 类型统一用 `ShaderResourceType`（含 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC`，batch tier 用）；`DescriptorType` / `DescriptorBindingFlags` 已删除。

## DescriptorEncoder 批量写入

直接 RHI/旧宿主的 ResourceGroup 更新统一走 **`DescriptorEncoder`**（`aurora/rhi/DescriptorEncoder.h`），`ResourceUpdateInfo` + `ResourceGroup::Update(vector)` 已移除。新 FrameContext 图路径由各 native compiler 在 Compile 内写入 lease-owned descriptor，worker 只消费冻结绑定，不重用旧宿主的隐式 ring：

- 接口：`ResourceGroup::CreateEncoder()` 返回 `std::unique_ptr<DescriptorEncoder>`；`WriteBuffer(binding, buffer, offset, range, arrayElement=0)` / `WriteImage` / `WriteSampler` + `End()` 提交。
- **Vulkan**：`VulkanResourceGroup` 持持久化 packed `mWriteInfos` + `mDirty`；`VulkanShader` 为每个 set 建 `VkDescriptorUpdateTemplate`（core 1.1，1.3 floor 下始终可用）。`End()` 仅 dirty 时 `vkUpdateDescriptorSetWithTemplate`；template 创建失败回退 `vkUpdateDescriptorSets`。DYNAMIC + `range==0` 仍 assert/warning（`aurora-dynamic-ubo-pack` 约定）。
- **DX12**：无批量 flush，descriptor heap 写入即时；动态绑定（`*_DYNAMIC`）记录到 `dynamicBindings`，bind 时走 root CBV/UAV。`End()` 是 no-op（接口对称）。
  - **shader-visible 隔离**：`D3D12DescriptorAllocator` 用 CPU-only staging heap（source of truth）+ `ringSize` 张 shader-visible heap（每 in-flight frame 一张，offset 1:1）；encoder 只写 staging，`BindResourceGroup` 前 `EnsureFrameCopy()` 用 `CopyDescriptorsSimple` 拷到当前帧 heap；`D3D12DeviceFrameContext::BeginFrame` 调 `allocator->BeginFrame(mFrameIndex)` 轮换 ring。
- **Metal**：直接绑定模型（无 descriptor 对象）。`MetalDescriptorEncoder`/`MetalDescriptorBatch` 即时写 `MetalResourceGroup` 的绑定表（`Write* 的 arrayElement` 映射到槽位 `binding + arrayElement`，与 slang MSL 数组摊平一致），`BindResourceGroup` 时 `setBuffer/setTexture/setSampler` 应用。`*_DYNAMIC` binding 由 shader reflection 派生，bind 时按 dynamicOffsets 顺序重定 offset（与 DX12 root CBV/UAV 契约一致）。

## DescriptorBatch 跨 set 批量

帧内跨 set 的 descriptor 写入走 **`DescriptorBatch`**（`aurora/rhi/DescriptorBatch.h`）：

- `Device::CreateDescriptorBatch()` 返回后端 batch；`WriteBuffer/WriteImage/WriteSampler(group, binding, ...)` 累积，`Flush()` 一帧一次，`Reset()` 帧末复用。
- **Vulkan** `VulkanDescriptorBatch`：累积 `VkWriteDescriptorSet`（`dstSet` = 各 group 当前 set），`Flush()` 单次 `vkUpdateDescriptorSets`。
- **DX12** `D3D12DescriptorBatch`：thin，`Write*` 复用 `D3D12DescriptorEncoder`（即时写 CPU staging），`Flush()`/`Reset()` no-op（copy 在 bind）。
- **Metal** `MetalDescriptorBatch`：thin，与 encoder 一样即时写绑定表，`Flush()`/`Reset()` no-op。
- 与 `DescriptorEncoder`（per-set template 路径）并存：单 set 快路径走 encoder，跨 set 批量走 batch。

## Dynamic UBO pack（batch tier / set 2）

Batch tier（set 2）用 dynamic UBO 承载 per-object uniform 数据，契约如下：

- **stable binding**：batch RG 的 dynamic UBO descriptor 每帧以 `offset=0, range=blockSize` 绑定一次（`blockSize` 为该 RG 对应 shader block 的固定大小）；帧内每 draw 只改 dynamic offset（`DrawItem::batchDynamicOffset`），不重写 descriptor。Vulkan 后端对 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC` + `bufferRange==0` 会 assert/warning，不得静默退化为 `VK_WHOLE_SIZE`。
- **`BatchAllocator` 是纯线性分配器**（`aurora/rdg/BatchAllocator.h`）：`Allocate/Write/Reset`，offset 对齐取自 `DeviceCapability::minUniformBufferOffsetAlignment`；**不感知帧**（无 frame index / slot）。
- **in-flight 安全由 `DeviceFrameContext` 维护**：每个 in-flight frame 一个 pack buffer（packed pool），帧 fence 完成后回收并 `Reset` 复用；随 `aurora-renderer` 落地，allocator 本身不实现回收/扩容。
- **batch RG 由各特性创建**（如「材质→shader」），不在 RHI/allocator 层；pipeline 层 `BatchPackWriter`（`aurora/pipeline/BatchPackWriter.h`）负责「结构体写入 → 返回 dynamic offset」。
- **注意**：`ShaderResourceType::UNIFORM_BUFFER_DYNAMIC` 当前**不**由 slang 反射产出（`ShaderCompilerSlang` 把 ParameterBlock 都映射为静态 `UNIFORM_BUFFER`）；DYNAMIC 标记由调用方 / 后续 compiler change 提供，测试中用「手工改 reflection 类型」验证 RG 语义。

## Vulkan dynamic rendering 与 stencil

`VulkanGraphicsEncoder::BeginRendering` 处理 depth + stencil：
- format 含 depth 时挂 `pDepthAttachment`
- format 含 stencil（D24_S8 / D32_S8）时挂 `pStencilAttachment`，
  load/store op 来自 `DepthStencilAttachment::stencilLoadOp/stencilStoreOp`
- `clearValue` 同时被 depth 与 stencil attachment 共享

format 的 hasDepth/hasStencil 通过 `GetImageFormatInfo(pixelFormat)` 查询。

## 平台/特性下限

- **Vulkan**：要求 1.3，`dynamicRendering` + `timelineSemaphore` 强制；PSO 从 `PipelineState::vertexBindings/vertexAttributes` 建 IA 输入布局（与 DX12 对齐）；tier2 `DescriptorHeap`（`VK_EXT_descriptor_heap`）**未实现**，`CreateDescriptorHeap` 返回 nullptr 且 `GetFeature().descriptorHeap == false`
- **DX12**：12.0 起步；PSO / root signature / ResourceGroup / SwapChain 均已落地，encoder 支持 render target 绑定与 clear、IA 顶点输入、indirect draw/dispatch、内置 fullscreen blit 与 tier2 bindless `DescriptorHeap`（SM6.6 门），见 `aurora-dx12-gaps`
- **Metal**：3 起步；PSO 从 `vertexBindings/vertexAttributes` 建 `MTLVertexDescriptor`；**buffer 槽位分区**——Metal 每 stage 31 个 `[[buffer(N)]]` 槽（0..30），shader buffer 绑定（含 push constant 的 declare-last 最高槽）占低段 `[0, 15)`，顶点缓冲占高段 `[15, 31)`（`METAL_VERTEX_BUFFER_SLOT_BASE = 31 - MAX_VERTEX_BINDINGS`，`BindVertexBuffers` 与 vertex descriptor 一致使用）；shader buffer 槽数超 15 在 pipeline 创建时 assert + 报错。push constant 支持 partial-range（CPU 侧 staging block 累积后整块 `set*Bytes` 重传）。Blit：同格式同尺寸走 blit encoder 原生 copy，缩放/过滤走 `MetalBlitHelper` 的 fullscreen-triangle render pass（dst 需 RENDER_TARGET usage）；Resolve 走 `MTLStoreActionMultisampleResolve` 空 render pass；两者由 `MetalBlitEncoder` 挂起/恢复 blit encoder 包住。SwapChain：3-image ring（`maximumDrawableCount=3`），`framebufferOnly=NO`（backbuffer 可采样）；acquire 仍是 CPU 立即 signal（Metal 无 GPU acquire 语义）。压缩格式：BC1-7（桌面 GPU）+ ASTC（Apple GPU）已映射，ETC2 无 Metal 对应。tier2 `DescriptorHeap` **未实现**：`DescriptorHeap` 接口按 `VK_EXT_descriptor_heap` 设计为**索引型堆**（tex/buf/smp 各自 backing buffer + per-type stride），而 slang 在 Metal 上走的是**指针/地址型 bindless**（`DescriptorHandle<T>` → `device T*` / `ulong`，无 argument buffer 的 `[[id]]` 形态）——两者模型不同。slang 对 Metal bindless `DescriptorHandle<T>` 的 **compiler emit 已完成**（slang #10842；本仓库 pin 的 v2026.9.2 已含），缺口在 (a) 我们未采用 `DescriptorHandle<T>` 的 shader/RHI 模型、(b) Metal 运行时实现、(c) 纯 `uniform` 数组-of-resource 仍有 bug（slang #12291，Q3 2026）。故 `feature.descriptorHeap == false`、`CreateDescriptorHeap()` 返回 nullptr。落地路线见 `openspec/changes/aurora-metal-bindless-descriptor-heap/`

## Metal 后端实现约定（ARC / 句柄 / 线程池 / MSL 槽位）

- **ARC**：`Aurora.Metal` 的 `.mm` 单独开启 `-fobjc-arc`（`metal/CMakeLists.txt` 对 `src/*.mm` 设 `set_source_files_properties(... "-fobjc-arc")`）。**不得**混用手工 `retain/release/dealloc`（ARC 下编译报错）。不要给整个后端加 `-fobjc-arc` 到 `.cpp`（`MetalRDGBackend.cpp` 是纯 C++）。
- **native 句柄**：Obj-C 类型不进头文件，句柄一律 `void*`。跨越 `id` ↔ `void*` 必须用 bridge cast：存句柄 `(__bridge_retained void *)obj`（+1，脱离 ARC），读句柄 `(__bridge id<MTLXxx>)handle`（借用），释放 `(void)(__bridge_transfer id<MTLXxx>)handle`（+1 交还 ARC）。**每个 `__bridge_retained` 必须恰有一个配对的 `__bridge_transfer`**（否则泄漏；MRC 下这些 cast 是 no-op，正是历史泄漏根因）。
- **Device 工厂**：统一走 `MetalDevice.mm` 内部模板 `CreateAndInit<T>(device, desc)`（`new T` + `Init` + 失败 `delete`），不逐方法复制样板。
- **MSL 槽位**：`src/MetalMSLSlots.h` 的 `BuildMetalMSLSlotLayout(reflection, set)` 是**唯一**来源——slang MSL 按类别（buffer/texture/sampler）各自从 0 顺序编号，忽略 `[[vk::binding]]`；`MetalShader`（push constant 槽 + buffer 预算）与 `MetalResourceGroup`（类别绑定槽映射）共用它。
- **线程池 / autorelease pool**：主线程帧池在 `MacosPlatform::PollEvent`（帧首建、次帧首 drain）；worker 线程池在 `MetalThreadContext::OnAttach/OnDetach` 用 `objc_autoreleasePoolPush/Pop`（ARC 禁 `NSAutoreleasePool`）。**ARC 不自动建池**；任何创建 autoreleased 对象的线程都必须有池。需跨帧/等 GPU 的对象**不可**依赖 autorelease pool（池 drain 早于 GPU 完成），必须显式强持有（`__bridge_retained` 句柄或 Metal 内部持有），GPU 完成信号用 `encodeSignalEvent` + fence（见 `MetalQueue::PendingUpload`）。

## Queue / Submit / Semaphore / SwapChain（submit-present）

提交与呈现路径（`aurora-queue-submit-present` + `aurora-client-viewport`）。

### Queue::Submit 契约

- 新 Renderer 接入使用 `Queue::TrySubmit`：Accepted 仅表示接受；Rejected 保证无入队；Indeterminate 必须保留引用并隔离到安全完成/teardown。Vulkan/DX12 已实现，Metal 新入口当前明确 Unsupported。
- 新路径使用 `Fence::Query` 区分 Pending/Signaled/Failed、`TryReset` 读取 reset 错误。Reset 不产生 submission；拒绝后的 Pending 不能作为等待理由。旧 BeginFrame/EndFrame 尚未迁移为 slot lease，不允许空转复用未提交的 fence。

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
- inflight frame 全局化：`DeviceFrameContext` 是 slot ring 的唯一所有者；新路径经 lease、Submit/PollCompleted 驱动。直接宿主 Begin/End/GetFrameFence 与 lease 在一个 context 内禁止混用；多 viewport 共用同一 lease。
- RDG 绑定 viewport：`RenderGraph::BindViewport(name, viewport)`（`ViewportImageTag`），prepare 阶段（`CullPasses` 开头）`Acquire` 解析裸 `Image*`（无 refcount，避开 swapchain image 的 `unique_ptr` 所有权冲突）。

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

## RDG 用法

RDG 是 **RHI 接口的扩展**：接口层（`aurora/rhi/interface/include/aurora/rdg/` + `src/rdg/`）持有
graph 结构、setup、以及后端无关的分析（依赖边 / 拓扑 / 生命周期 / culling / transient 池），
**每个后端通过 `RDGBackend` 接口自己实现 barrier 编译器与 executor**（`CompileBarriers` / `Execute`）：

| 后端 | 实现 |
|---|---|
| Vulkan | `vulkan/src/rdg/VulkanRDGBackend.{h,cpp}` |
| DX12 | `dx12/src/rdg/D3D12RDGBackend.{h,cpp}` |
| Metal | `metal/src/rdg/MetalRDGBackend.{h,cpp}` |

创建入口：`RenderGraph::Build(device)`（`RenderGraph` 是具体类，构造时经 `Device::CreateRDGBackend()` 取后端，不搞多态图继承）。

三段式 lifecycle：

1. **Setup**：`RenderGraph::Build(device)` → `CreateTexture/CreateBuffer/Import` → `AddRasterPass/AddComputePass/AddCopyPass`（setup callback 内通过 builder 声明 `Read/Write/ColorAttachment/DepthStencilAttachment/Src/Dst`）
2. **Compile**：`Compile()` 做共享分析（依赖边 / 拓扑 / 生命周期 / culling / transient），再回调后端的 `CompileBarriers()` 做 barrier 推导
3. **Execute**：`Execute(cmdBuf)` 回调后端的 `ExecuteImpl()`（默认走共享的 `ExecutePasses` 按拓扑序 emit barrier + encoder + pass body，v1 单一 CommandBuffer）

要点：

- **handle 强类型**：`RDGTextureHandle` / `RDGBufferHandle` 是 opaque ID，不可混用；仅在所属 graph 生命周期内有效
- **execute lambda 不得手写 barrier**：所有 barrier 由 RDG 提供，execute lambda 内调用 `cmdBuf->PipelineBarrier` 是未定义行为
- **execute lambda 的 capture 生命周期**：必须在 `Execute()` 返回前保持所有 captured 引用有效
- **barrier 推导辅助在接口层**：`InferLayoutForAccess` / `InferStageForAccess`（`aurora/rhi/Barrier.h`），后端 barrier 编译器复用或覆盖
- **transient 池**：对象池（v1，按完整 desc 复用整 Image）+ 堆池（v2 预留，memory heap aliasing）
- 低复杂度场景（demo / tool）直接用 Encoder + 手写 `PipelineBarrier` 仍是合法路径

## 骨架 / 蒙皮（animation 独立，aurora 无 Skeleton）

`Skeleton` 属于独立模块 `engine/animation`（target `Animation`，只依赖 `Core`），**aurora 不定义也不依赖它**。旧引擎同理：`render/core` 自持 `Skin`，只有 `render/adaptor` 才 link `Animation`。

| 类型 | 位置 | 含义 |
|---|---|---|
| `Skin` | `aurora/resource/Skin.h` | mesh 侧蒙皮绑定（`inverseBindMatrices` + `boneMatrices` + `boneMapping`），自持 |
| `SkinnedMesh`（scene 组件） | `aurora/scene/SceneTypes.h` | 实例：持 `CounterPtr<Skin>` |

约定：

- **不要在 aurora 里定义 `Skeleton` / `Bone`**，也不要新增 `aurora/animation` 之类目录；`aurora/core` 不依赖 `Animation`。
- `animation::Skeleton`（+ pose）→ `aurora::Skin::boneMatrices` 的映射放在**桥接层**（未来 aurora adaptor / framework），由桥接层 link `Animation`。
- 渲染侧每帧求值若需独立类型，叫 `SkinningPalette` / `SkinMatrices`，不要占用 `Skin`。
- `Mesh` 只有 `SetSkin/GetSkin/HasSkin`。

## 后续 change 路线

| Change | 状态 | 说明 |
|---|---|---|
| `aurora-quick-fixes` | ✅ 已实施 | 本文档所述默认值/命名 |
| `aurora-queue-submit-present` | ✅ 已实施 | Queue / Submit / SwapChain Present（3 后端） |
| `aurora-renderpass` | ✅ 已实施 | `FullScreenPass` 基类 + `ScenePass`(HDR) + `TextureToScreenPass`（renderpass begin/end 契约） |
| `aurora-client-viewport` | ✅ 已实施 | `RenderViewport` 表面 + `ClientViewport` + `RenderGraph::BindViewport` + inflight frame 全局化 |
| `aurora-encoder-barriers` | ✅ 已实施 | `CommandBuffer::PipelineBarrier`（最终落在 cmdbuf 而非 encoder） |
| `aurora-resource-group` | ✅ 已实施 | ResourceGroup / 描述符绑定（Vulkan + DX12 + Metal 直接绑定，dynamic offset 三后端齐） |
| `aurora-remove-resource-group-layout` | ✅ 已实施 | 移除 ResourceGroupLayout，ResourceGroup 从 shader reflection 派生 |
| `aurora-renderer` | Windows 主流程与宿主已接入 | Vulkan 双 compiler / DX12；Metal 对等仍待 P02 |
| `aurora-rdg` | ✅ 已实施 | render graph（三段式 RDG） |
| `aurora-dynamic-ubo-pack` | ✅ 已实施 | Batch tier dynamic UBO stable binding + `BatchAllocator` 线性分配 + `BatchPackWriter` |
| `aurora-launcher-integration` | ✅ 已实施（最小闭环） | `AuroraRender` 动态模块（`IModule` + `REGISTER_MODULE`）+ 窗口/swapchain/clear/present 闭环；launcher 只加载 aurora，移除 legacy `SkyRender`/`RenderAdaptor`；引擎内置 `configs/` |
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
- 模块 Init 失败先 Shutdown/销毁，依赖消费者不初始化，Application 返回失败。跨 DLL 服务仍用 Singleton，注册幂等仍用 Once。
- Windows 旧 AuroraModule GPU loop 和旧 EditorRenderer 编译排除。未验证平台保留整套旧入口；Metal 的新编译/输出/宿主协议仍待 P02，不能借用新 Renderer 的设备另起旧 loop。

## Image 边界状态

- ImageBoundaryState 的显式表必须完整覆盖 layer*mipLevels+mip，统一 layout 与全等数组语义相同；final 不接受 UNDEFINED，已有 texel 不能位于 initial UNDEFINED 的 cell。状态布局不扩大内容资格。
- FrameContext 从 Image::GetMipLayerCount 查询已确认归属的实际 backing cell 数；默认 trackedImageSubresources=65,536，和 backing 数分别限额，owner/GPU 安全退休才归还。混合快照有复制成本，仍由 coordinator 构建；跨队列、aspects/planes 与 Metal 新协议未开放。

## Raster attachment view

- RasterAttachmentView 独立于采样 view，color/depth 各选一个 mip/layer，默认 0/0；LOAD/STORE/CLEAR 和内容依赖只影响该 cell，scissor 与 color/depth 匹配按所选 mip extent，整图 export 不扩大资格。
- Vulkan/DX12 的 native view/RTV/DSV 在 Compile 构造并随计划/提交退休，workers 不创建；RenderPass 合并同时比较逻辑附件和两个 view。当前仅 2D；RGBA8 可用设备支持的 2/4/8× color MSAA/resolve，D32 支持匹配 color 采样数的单 sample 或 2/4/8× MSAA；同 pass attachment feedback 仍整 image 拒绝。

- B10 第十六子批的 colorResolve 是 raster 内部独立写入；源 CLEAR/LOAD 保证全样本有效，源丢弃不影响 resolve 结果，目标可选非零 mip/layer。MSAA storage 限 GPU_ONLY/单 mip；RGBA8 仅 RENDER_TARGET，D32 仅 DEPTH_STENCIL，分别预检设备能力且 color/depth extent 与 samples 匹配；不开放 depth resolve、采样、buffer copy 或局部初始化。上传回执把 resolve target 计为额外写入。
