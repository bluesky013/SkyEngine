# aurora-pipeline Specification

## Purpose
TBD - consolidated from: aurora-renderpass aurora-render-viewport aurora-scene-collect pipeline-pass-template
## Requirements
### Requirement: Render pass begin/end 声明契约

Pipeline feature SHALL 通过 FrameGraphBuildContext 声明 raster attachments、load/store、clear、view 和 draws；对应后端 Compile SHALL 生成合法 native rendering scope，Dispatch 按已编译计划编码。feature 不继承旧 PipelinePass，不手写后端 begin/end。

#### Scenario: 声明生成 native scope
- **WHEN** feature 为当前 View 声明 color/depth attachments 和 draws
- **THEN** 后端编码的 attachment、load/store、render area 与声明一致，并维持编译后的绘制顺序

### Requirement: RenderViewport 三阶段生命周期契约

`RenderViewport` SHALL 定义三阶段生命周期：`Begin()`（check swapchain 状态 + 重建，false = 取消当帧 present）、`Acquire()`（拿下一帧 backbuffer，false = 本帧不可渲染）、`Release()`（present，取消帧为 no-op）。

`RenderViewport` SHALL 暴露：`GetBackbuffer()`（Acquire 成功后有效）、`GetFormat()`、`GetExtent()`、`GetName()`，以及供 Submit 用的 `GetAcquireSemaphore()` / `GetRenderDoneSemaphore()`（当前帧 ring 的 BINARY sema）。基类 SHALL NOT 暴露 frame index 或 fence（全局化，见「全局 inflight frame」需求）。

#### Scenario: Begin 失败取消 present

- **WHEN** `viewport->Begin()` 返回 false（swapchain 状态异常且重建失败）
- **THEN** 帧驱动方跳过本帧（不建 RDG、不 Submit、不 Present）

#### Scenario: Acquire 后 backbuffer 有效

- **WHEN** `viewport->Begin()` 成功且 `viewport->Acquire()` 返回 true
- **THEN** `GetBackbuffer()` 返回非空 `Image*`（持有 RENDER_TARGET），`GetFormat()`/`GetExtent()` 与表面一致

### Requirement: ClientViewport 持有 SwapChain 与 per-image sema

`ClientViewport` SHALL 继承 `RenderViewport`，`Init(Device*, const SwapChain::Descriptor&)` 创建 SwapChain 并分配 `imageCount` 组 acquire/render-done BINARY semaphore（按 per-viewport 帧 ring 索引，本地 acquire 计数器）。

`Begin()` SHALL 依据 `GetStatus()` 重建；`Acquire()` SHALL 调 `AcquireNextImage(acquireSema[slot], nullptr, UINT64_MAX)` 并记 image index；`Release()` SHALL 在帧有效时调 `Present(imageIndex, 1, &renderDoneSema[slot])`。`ClientViewport` SHALL NOT 持有 frame index / fence（全局化）。

#### Scenario: Acquire 返回有效索引

- **WHEN** `ClientViewport::Init` 成功后 `Begin()` + `Acquire()`
- **THEN** `Acquire()` 返回 true；backbuffer 非空；acquire sema / render-done sema 均非空

#### Scenario: 取消帧的 Release 为 no-op

- **WHEN** `Begin()` 或 `Acquire()` 失败后调用 `Release()`
- **THEN** 不调用 `SwapChain::Present`，不崩溃

### Requirement: SwapChainStatus 自查与重建

`ClientViewport::Begin()` SHALL 查询 `SwapChain::GetStatus()`：`OK` → 直接继续；`OUT_OF_DATE` → 调 `Resize` 重建；`LOST` 或 `Resize` 失败 → 取消当帧 present。surface 尺寸由原生窗口决定，RHI 层不决定目标尺寸；`SUBOPTIMAL` SHALL 当作 `OK` 处理。

#### Scenario: OUT_OF_DATE 触发重建

- **WHEN** 窗口尺寸变化导致 `GetStatus()` 返回 `OUT_OF_DATE`
- **THEN** `Begin()` 触发 `Resize`；重建成功后本帧正常渲染；`GetExtent()` 反映新尺寸

#### Scenario: 重建失败取消 present

- **WHEN** `GetStatus()` 返回 `LOST` 或 `Resize` 返回失败
- **THEN** `Begin()` 返回 false；本帧 present 被取消，不崩溃

### Requirement: 全局 inflight frame 共享

FrameScheduler SHALL 复用 DeviceFrameContext 唯一 slot ring，通过 DeviceFrameLease 编排生产图的分配、编译和提交。同一帧的多个 View/viewport 使用同一 lease，各自可变 uniform 与 descriptor 范围 SHALL 不重叠；slot 只有在既有退休协议允许后才复用。直接 RHI 宿主的 BeginFrame/EndFrame 接口 SHALL 不与同一 context 的 lease 协议混用。

#### Scenario: 多 View 常量隔离
- **WHEN** 两个 View 在同一 lease 写入不同相机和 Batch 值
- **THEN** 各自读到独立范围，后建图或下一帧不能覆写仍在途数据

### Requirement: 各后端 present 映射

`ClientViewport` SHALL 只依赖 `SwapChain`/`Queue`/`Semaphore`/`Fence` 抽象，后端无关。present 差异封闭在各后端 `SwapChain` 实现：

- Vulkan：acquire `vkAcquireNextImageKHR` signal binary；render-done submit signal binary；present `vkQueuePresentKHR` wait。
- DX12：acquire `GetCurrentBackBufferIndex`（acquire sema 立即 signal）；render-done `ID3D12Fence::Signal`；present `IDXGISwapChain3::Present`。
- Metal：acquire `nextDrawable` + 立即 signal；render-done `MTLEvent` signal；present `presentDrawable:` 前 `encodeWaitForEvent`。

#### Scenario: 三后端接口一致

- **WHEN** 同一 `ClientViewport` 调用序列跑在 Vulkan / DX12 / Metal 任一后端
- **THEN** `Begin`/`Acquire`/`Release` 接口与顺序一致，后端差异不泄漏到调用方

### Requirement: SceneView（aurora）

`SceneView` SHALL 提供 frustum（6 plane）+ view/viewProject 矩阵，以及 `FrustumCulling(const BoundingBoxSphere&) -> bool`。v1 只为 culling 与排序服务，不涉及常量上传。

#### Scenario: frustum 剔除

- **WHEN** primitive 的 worldBounds 完全在 view frustum 外
- **THEN** `FrustumCulling(bounds)` 返回 false，该 primitive 被剔除

### Requirement: RenderScene（aurora）

`RenderScene` SHALL 内嵌 `EntityRegistry`，提供 entity 注册与 SoA 组件池能力（`CreateEntity` / `DestroyEntity` / `Add<T>` / `Get<T>` / `Pool<T>` / `View<Ts...>`）；views（SceneView）保留独立 registry 不进 ECS。

场景组件 SHALL 包括：

- `Bounds`（`BoundingBoxSphere`）
- `WorldInfo`（纯 world 矩阵：`Matrix4 world`，默认 Identity；不拆 TRS）
- `Light`（声明于 `aurora/light/LightTypes.h`，由 `aurora/scene/SceneTypes.h` 转发：type/color/intensity + point/spot 参数 `range` / `innerConeAngle` / `outerConeAngle`；**不**存储 `position` / `direction`，世界位置 / 朝向由 `WorldInfo` 变换派生）
- `MainLight`（`aurora/light/LightTypes.h`，注册组件：把某个 `Light{DIRECTIONAL}` entity 标记为场景主光，携带 `castShadow` 等；`RenderScene` **不**缓存主光）
- `Skin`（占位）

#### Scenario: ECS 组件挂载

- **WHEN** `scene.CreateEntity()` 后 `scene.Add<Light>(id, {...})`
- **THEN** Light 存入对应 SoA 池；`scene.DestroyEntity(id)` 后移除

#### Scenario: 场景组件独立可用

- **WHEN** 调用方通过 `View<Bounds>` 读取 scene registry
- **THEN** 仍可读取其 CPU bounds；这不构成生产 Renderer 的隐式收集入口

#### Scenario: point/spot 参数

- **WHEN** `Light{type=POINT, range}` 或 `Light{type=SPOT, range, innerConeAngle, outerConeAngle}`
- **THEN** 各参数完整存储于组件；位置 / 朝向不存于组件，而从同 entity 的 `WorldInfo.world` 派生

#### Scenario: WorldInfo 矩阵存储

- **WHEN** `scene.Add<WorldInfo>(id, {matrix})` 后 `scene.Get<WorldInfo>(id)`
- **THEN** 读回的 world 矩阵与写入一致

### Requirement: Renderer 逐 View 最小绘制管线

新 Renderer 路径 SHALL 从冻结 Scene/View 数据提取 owned draw packets，按 View 独立执行相机常量、裁剪、LOD、tag 与稳定排序；coordinator SHALL 解析精确 mesh/material 版本并在单帧图中构建 raster 与合成。首版已支持的附件范围 SHALL 为 RGBA8 color 与可选 D32 depth；未支持的 HDR/格式 SHALL 返回明确错误。旧可变 pass 模板已移除；场景和 fullscreen 由注册的 FrameGraph feature 构建。

#### Scenario: 同 Scene 两个相机
- **WHEN** 两个 View 读取相同 Scene 并使用不同 projection、extent 或 tag
- **THEN** Scene prepare 只执行一次，逐 View draw 与常量相互独立，片段完成顺序不改变稳定结果

#### Scenario: 精确版本与逐 View 常量
- **WHEN** 本帧已冻结 mesh/material 版本，后续 packet 更新资源或相机
- **THEN** 当前帧保留原 geometry、layout、backing 和常量，不能解析成更新版本或改写在途 descriptor

### Requirement: Renderer 输出合成与统一 slot

FrameScheduler SHALL 管理窗口与离屏输出，复用 DeviceFrameContext 的唯一 slot ring。BeginViewport SHALL 查询 surface 配置；CPU prepare 和 slot 就绪后才为输出 pass acquire，并复核配置。FrameComposer SHALL 用带 generation/revision 的 ViewOutput 在同一图组合场景、UI 和输出；成功 acquire 的同步与 presentation 责任 SHALL 独立保活至合法退休。

#### Scenario: 不可用窗口保留其它工作
- **WHEN** 一个或所有窗口无法 acquire，但存在共享上传或离屏绘制
- **THEN** 只省略相应输出 pass，其余命令继续由同一 context 提交，不使用 fake backbuffer

#### Scenario: 双窗口与离屏的有界分配
- **WHEN** 多个 View 在同一 lease 请求常量、staging 或 descriptor
- **THEN** 分配范围互不覆盖，超额显式失败，不能通过重置 slot 或借用其它在途分区掩盖容量不足

### Requirement: 单一生产图入口与旧 API 退役

Aurora SHALL 通过 FrameComposer/FrameGraphBuildContext、FrozenFrameGraph 和 DeviceFrameContext 编排图渲染。旧 RenderGraph、CompiledGraph、RDGBackend/CreateRDGBackend、PipelinePass 模板及其专属 GlobalRenderResources/BatchAllocator/BatchPackWriter SHALL 不再构建或暴露。共享 shader 声明、RgBlockDesc/UniformUploadPlan、SceneRenderFeature、TextureToScreenFeature、FrameTransientPool 和直接 RHI/viewport 接口 SHALL 保持可用。

#### Scenario: 无兼容执行旁路
- **WHEN** 构建 RHI、Pipeline、三个后端和宿主
- **THEN** 不引用或导出旧图接口，生产图仍由各 backend 的 frame compiler 编译

#### Scenario: 共享声明保留
- **WHEN** C++ 与 shader 使用 OpaquePass/ScenePass/TextureToScreenPass 的共享声明文件
- **THEN** 原有资源接口与导出路径保持有效，删除同名旧 C++ pass 类不删除声明源
