## MODIFIED Requirements

### Requirement: Render pass begin/end 声明契约

Pipeline feature SHALL 通过 FrameGraphBuildContext 声明 raster attachments、load/store、clear、view 和 draws；对应后端 Compile SHALL 生成合法 native rendering scope，Dispatch 按已编译计划编码。feature 不继承旧 PipelinePass，不手写后端 begin/end。

#### Scenario: 声明生成 native scope
- **WHEN** feature 为当前 View 声明 color/depth attachments 和 draws
- **THEN** 后端编码的 attachment、load/store、render area 与声明一致，并维持编译后的绘制顺序

### Requirement: 全局 inflight frame 共享

FrameScheduler SHALL 复用 DeviceFrameContext 唯一 slot ring，通过 DeviceFrameLease 编排生产图的分配、编译和提交。同一帧的多个 View/viewport 使用同一 lease，各自可变 uniform 与 descriptor 范围 SHALL 不重叠；slot 只有在既有退休协议允许后才复用。直接 RHI 宿主的 BeginFrame/EndFrame 接口 SHALL 不与同一 context 的 lease 协议混用。

#### Scenario: 多 View 常量隔离
- **WHEN** 两个 View 在同一 lease 写入不同相机和 Batch 值
- **THEN** 各自读到独立范围，后建图或下一帧不能覆写仍在途数据

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

### Requirement: 单一生产图入口与旧 API 退役

Aurora SHALL 通过 FrameComposer/FrameGraphBuildContext、FrozenFrameGraph 和 DeviceFrameContext 编排图渲染。旧 RenderGraph、CompiledGraph、RDGBackend/CreateRDGBackend、PipelinePass 模板及其专属 GlobalRenderResources/BatchAllocator/BatchPackWriter SHALL 不再构建或暴露。共享 shader 声明、RgBlockDesc/UniformUploadPlan、SceneRenderFeature、TextureToScreenFeature、FrameTransientPool 和直接 RHI/viewport 接口 SHALL 保持可用。

#### Scenario: 无兼容执行旁路
- **WHEN** 构建 RHI、Pipeline、三个后端和宿主
- **THEN** 不引用或导出旧图接口，生产图仍由各 backend 的 frame compiler 编译

#### Scenario: 共享声明保留
- **WHEN** C++ 与 shader 使用 OpaquePass/ScenePass/TextureToScreenPass 的共享声明文件
- **THEN** 原有资源接口与导出路径保持有效，删除同名旧 C++ pass 类不删除声明源
