# aurora-pipeline Specification

## Purpose

Define render pipeline composition, lighting and pass behavior.

## Scope notes

### aurora-pipeline

Specify the production frame feature pipeline, scene/view inputs and shared output lifecycle.

## Requirements

### Requirement: Light 子模块布局与类型归属

light 类型与服务 SHALL 位于 `aurora/core` 的 light 子模块（非独立 target，编入 `Aurora.Core.Static`）：

- `aurora/light/LightTypes.h` SHALL 声明 `LightType` / `Light` / `MainLight` / `LightRenderData`，并包含 `Light` 与 `MainLight` 的 `SKY_TYPE_TAG`。
- `aurora/light/LightSystem.h` SHALL 声明 `LocalLightSystem` 与 `ExtractLightPosition` / `ExtractLightDirection`。
- `aurora/light/LightShell.h` SHALL 声明影响体积 / shell / 覆盖处理函数。
- `aurora/scene/SceneTypes.h` SHALL 通过 include `aurora/light/LightTypes.h` 转发 light 类型，不再本地声明。

#### Scenario: 头转发保持兼容
- **WHEN** 仅 `#include <aurora/scene/SceneTypes.h>` 后使用 `sky::aurora::Light`
- **THEN** 编译通过（类型经转发可见）

#### Scenario: 组件类型可注册
- **WHEN** 对 `MainLight` 调用 `TypeId<MainLight>()`
- **THEN** 编译通过（已注册 `SKY_TYPE_TAG`）

### Requirement: MainLight 主方向光组件

`MainLight` SHALL 是注册的 ECS 组件，携带主光专属数据（至少 `bool castShadow`，默认 `true`），用于把某个 `Light{DIRECTIONAL}` entity 标记为场景主光；其颜色 / 强度取自同实体的 `Light`，世界朝向取自同实体的 `WorldInfo`。`RenderScene` SHALL NOT 持有或缓存主光（无 `Get/SetMainLight`）。

#### Scenario: 主光作为组件挂载
- **WHEN** `scene.Add<MainLight>(id, MainLight{})` 且该 entity 有 `Light` / `WorldInfo`
- **THEN** `scene.Get<MainLight>(id)` 非空；`View<MainLight, Light, WorldInfo>` 迭代到该 entity

#### Scenario: RenderScene 不持有主光
- **WHEN** 检查 `RenderScene` 接口
- **THEN** 不存在 `MainLight` 成员或 `Get/SetMainLight` 访问器

### Requirement: LocalLightSystem 收集 LightRenderData

`LocalLightSystem::Process(RenderScene&, std::vector<LightRenderData>& out)` SHALL 遍历 `View<Light, WorldInfo>`，对每个光源派生世界位置 / 朝向并打包 `LightRenderData` 追加到 `out`；`out` SHALL 由调用方持有（不在 scene 上缓存）。打包 SHALL 为：`position = (pos, type)`、`color = (rgb, intensity)`、`direction = (dir, 0)`、`params = (range, innerConeAngle, outerConeAngle, 0)`。`Process` SHALL 无状态。

#### Scenario: 点光打包
- **WHEN** entity 有 `Light{type=POINT, color, intensity, range}` 与平移 `(1,2,3)` 的 `WorldInfo`
- **THEN** `out` 追加记录 `position == (1,2,3, POINT)`、`color.w == intensity`、`params.x == range`

#### Scenario: 聚光方向
- **WHEN** entity 的 `WorldInfo` 为 Identity（派生朝向 -Z）
- **THEN** 追加记录的 `direction.xyz` 约为 `(0,0,-1)`

#### Scenario: 调用方持有缓冲
- **WHEN** 对同一 scene 连续调用两次 `Process`
- **THEN** `out` 追加两轮记录（不覆盖）

#### Scenario: 无光源
- **WHEN** scene 中没有 `Light` 组件
- **THEN** `out` 不变

### Requirement: 光源位置与朝向由实体变换派生

`ExtractLightPosition(const Matrix4&)` SHALL 返回矩阵平移列 `m[3].xyz`；`ExtractLightDirection(const Matrix4&)` SHALL 返回归一化后的矩阵旋转作用于局部前向 `Vector3::VEC3_NZ`（退化矩阵回退为 `VEC3_NZ`）。

#### Scenario: 位置来自平移列
- **WHEN** `world.m[3] == (10,20,0,1)`
- **THEN** `ExtractLightPosition` 返回 `(10,20,0)`

#### Scenario: 朝向来自旋转
- **WHEN** `world` 为绕 Y 轴 90° 的纯旋转
- **THEN** `ExtractLightDirection` 返回 `-Z` 经该旋转后的单位向量

### Requirement: LightVolume 影响体积分类

`ComputeLightVolume(const Light&, const WorldInfo&, const SceneView *view)` SHALL 按 `LightType` 分类：`DIRECTIONAL → BOX`、`POINT → SPHERE`、`SPOT → CONE`，并用 `WorldInfo` 派生位置 / 朝向。`POINT` 半径为 `range`；`SPOT` 的 `apex` 为派生位置、`axis` 为派生朝向、`height` 为 `range`、`baseRadius` 为 `range * tan(outerConeAngle)`；`DIRECTIONAL` 盒在 `view` 非空时覆盖视锥世界 AABB，`view` 为空时用大默认盒。

#### Scenario: 点光体积
- **WHEN** `Light{type=POINT, range=7}`、平移 `(1,2,3)`
- **THEN** `type == SPHERE`、`center == (1,2,3)`、`radius == 7`

#### Scenario: 聚光体积
- **WHEN** `Light{type=SPOT, range=10, outerConeAngle=0.5}`、朝向 -Z
- **THEN** `type == CONE`、`axis` 为单位 -Z、`baseRadius == 10 * tan(0.5)`

#### Scenario: 平行光体积
- **WHEN** `Light{type=DIRECTIONAL}` 且 `view != nullptr`
- **THEN** `type == BOX`，盒覆盖视锥世界 AABB

### Requirement: 世界空间保守包围

`ComputeLightBounds(const Light&, const WorldInfo&)` SHALL 返回包含光源影响体积的世界空间保守 AABB：点光为 `position ± range`；聚光同时包含 apex 与底面圆盘（底面轴向外扩 `baseRadius * sqrt(1 - axis_i^2)`）；平行光为大默认盒。`LightVolume::bounds`（球 / 锥）SHALL 与之一致。

#### Scenario: 球包围
- **WHEN** 点光位置 `(0,0,0)`、`range=5`
- **THEN** `min == (-5,-5,-5)`、`max == (5,5,5)`

#### Scenario: 锥包围包含底面
- **WHEN** 聚光 apex `(0,0,0)`、axis `-Y`、`height=4`、`baseRadius≈2`
- **THEN** `bounds` 覆盖 apex 与底面圆盘

### Requirement: Light shell 凸包几何

`GenerateLightShellGeometry(const LightVolume&, sectors, rings)` SHALL 返回 `sky::GeometryStreams` 凸包 shell：`SPHERE` 为球心 `center`、半径 `radius` 的球面；`CONE` 的 apex 位于 `volume.apex`、底面环中心位于 `apex + axis*height`、底面半径 `baseRadius`；`BOX` 由 `box` 缩放/平移的立方体。`NONE` SHALL 返回空 streams。

#### Scenario: 球 shell 顶点在球面
- **WHEN** `SPHERE{center=(0,0,0), radius=2}`
- **THEN** 每个 position 到原点距离约为 2

#### Scenario: 锥 shell apex 与底面
- **WHEN** `CONE{apex=(0,0,0), axis=(0,-1,0), height=4, baseRadius=2}`
- **THEN** 存在顶点在原点；底面环顶点 y 约为 -4 且到轴距离约为 2

### Requirement: 屏幕空间保守 2D 包围盒

`ComputeConservativeScreenRect(const LightVolume&, const Matrix4 &viewProj, const Vector2 &extent)` SHALL 投影 `bounds` 8 角点取屏幕 min/max；任一角点 `clip.w <= 0` 或体积为 `BOX` 时 SHALL 返回覆盖整屏的保守矩形（`valid == false`）。

#### Scenario: 屏内体积保守覆盖
- **WHEN** 位于相机前方的体积投影到屏幕
- **THEN** 返回矩形包含其真实投影（允许更大），`valid == true`

#### Scenario: 平行光整屏
- **WHEN** `volume.type == BOX`
- **THEN** 返回矩形覆盖 `[0,extent]` 且 `valid == false`

### Requirement: 屏幕空间保守 tile 覆盖

`ComputeConservativeCoverage(const LightVolume&, viewProj, extent, tileSize)` SHALL 对 shell 三角形做 tile 粒度保守光栅化：三角形屏幕 AABB 与 tile 相交即标记；返回 `tilesX` / `tilesY` 与覆盖 tile 线性索引列表，且 SHALL 为真覆盖的超集。`BOX` 或任一 shell 顶点在相机后时 SHALL 覆盖全部 tile。

#### Scenario: 覆盖为真覆盖超集
- **WHEN** 体积投影覆盖屏幕中心
- **THEN** 返回 tile 集合包含中心 tile

#### Scenario: 屏幕外体积空覆盖
- **WHEN** 体积完全在屏幕外
- **THEN** 返回覆盖列表为空

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

### Requirement: PreparedRaster 消费 Pass 契约

新 Renderer 的具体 Pass SHALL 决定资源布局与参数来源，并使用共享类型填充数据。PreparedRaster SHALL 负责冻结版本解析、mesh/LOD/range 校验和声明式 draw 组装，不得定义所有 Pass 必须使用的 View/Material/Object ABI。生产入口 SHALL 验证实际声明与 reflection 的一致性，不得要求固定三个 UBO、固定 80/64 字节或固定 Material=set 1。

#### Scenario: 声明驱动的生产绘制
- **WHEN** 生产 renderer 接收使用共享 Pass 布局的数据和匹配 shader
- **THEN** 组装并执行对应 draw，绑定数据来自该 Pass 声明，不从示例 ABI 补齐或改写

#### Scenario: 不匹配布局
- **WHEN** 提供的数据范围、字段布局或绑定类型不符合该 Pass 的 shader reflection
- **THEN** 在执行前返回明确错误，不截断、不重新解释为固定结构、不绕过反射校验

#### Scenario: 保留资源生命周期
- **WHEN** Pass 使用同图首次上传资源或旧版本回退资源
- **THEN** 精确版本 pins、InGraph 依赖、frame lease 与 GPU 退休行为保持有效，共享布局接入不得绕过这些协议
