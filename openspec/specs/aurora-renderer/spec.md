# aurora-renderer Specification

## Purpose

Define renderer lifecycle, scene adaptation, launcher integration, built-in geometry and level-of-detail behavior.

## Scope notes

### aurora-renderer

Specify the Aurora renderer interface, owned scene and viewport commands, parallel CPU preparation, exact resource versions, bounded GPU scheduling, frame composition and safe lifecycle behavior of supported hosts.

## Requirements

### Requirement: adaptor 静态库 + runtime/editor 动态模块

Aurora Framework 适配 SHALL 位于 engine/aurora/adaptor，目标为 Aurora.Adaptor.Static；Renderer foundation 实现 SHALL 位于 Aurora.Core.Static，核心不得依赖 Framework 或业务 plugin。运行时入口 SHALL 位于 engine/aurora/runtime 的 Aurora.Render；编辑扩展 SHALL 位于 engine/aurora/editor 的 Aurora.Render.Editor，注册编辑器功能而不建立第二个 Renderer。模块 SHALL 复用既有静态实现，支持配置指定的动态或 tagged static 入口。

#### Scenario: 既有职责与装载入口
- **WHEN** 构建并装载 Aurora.Render 与 Aurora.Render.Editor
- **THEN** 两者使用 Aurora.Adaptor.Static，Renderer 实现只归 Aurora.Core.Static；编辑扩展不重复创建 Device

#### Scenario: core 与 Framework 边界
- **WHEN** 构建 Aurora.Core.Static 或仅接口消费者
- **THEN** core 不链接 Framework，接口消费者不需要链接 foundation 的实现 target

### Requirement: AuroraReflection 入口

AuroraReflection SHALL register Aurora schema, asset handlers and components exactly once from Aurora.Metadata::RegisterMeta during process definition creation. Runtime, cook and editor modules SHALL depend on that owner; views SHALL only bind registered data. The function SHALL NOT be idempotent through Once, once_flag, static flags, existing-type checks or scratch nodes. A second registration owner SHALL remain a programming error caught by strict UUID/name checks.

#### Scenario: Runtime cook and editor share one definition
- **WHEN** these consumers load, restart or open several views in one process
- **THEN** AuroraReflection has one production call site and each schema is registered once

#### Scenario: Duplicate registration is rejected
- **WHEN** another owner attempts to register the same type UUID or name
- **THEN** the programming invariant fails rather than silently accepting or skipping the duplicate

### Requirement: 运行时资产层

SHALL 为 `aurora::Mesh` / `Material` / `MaterialInstance` / `Texture` / `LodGroup` 提供 `AssetTraits<T>`（`DataType` + `ASSET_TYPE` + `SERIALIZE_TYPE`）、asset data 结构及 `Json`/`Bin` 存取，并在 `AuroraReflection` 中 `AssetManager::RegisterAssetHandler<T>()`。本 change SHALL 只做运行时加载，SHALL NOT 接离线 `AssetBuilderManager` 构建产物。

#### Scenario: 注册资产处理器

- **WHEN** `AuroraReflection` 完成
- **THEN** `AssetManager` 已注册 `Mesh`/`Material`/`MaterialInstance`/`Texture`/`LodGroup` 的 handler

#### Scenario: 资产数据往返

- **WHEN** 序列化再反序列化对应 asset data
- **THEN** 字段一致

### Requirement: 资产驱动组件

组件类型 SHALL 位于命名空间 `sky::aurora` 且 SHALL NOT 再加 `Aurora` 前缀（命名空间已限定）。SHALL 提供 `StaticMeshComponent`（`StaticMeshComponentData{ Uuid mesh; Uuid material; castShadow; receiveShadow; }`）、`LodGroupComponent`（`LodGroupComponentData{ Uuid lodGroup; castShadow; receiveShadow; }`）、`CameraComponent`、以及按光源类型拆分的 `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`，均派生 `ComponentAdaptor<Data>` 并注册到 `ComponentFactory`（分组 `"Aurora"`）。资产引用成员 SHALL 用 `Uuid` 且 SHALL 以 `SET_ASSET_TYPE(AssetTraits<T>::ASSET_TYPE)` 标注。

#### Scenario: 组件注册到组件层

- **WHEN** 查询 `ComponentFactory::GetTypes()` 的 `"Aurora"` 分组
- **THEN** 含 `StaticMeshComponent` / `LodGroupComponent` / `CameraComponent` / `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`

#### Scenario: 资产成员带类型元数据

- **WHEN** 取 `StaticMeshComponent` 的网格成员节点或 `LodGroupComponent` 的 LodGroup 成员节点
- **THEN** 其 `CommonPropertyKey::ASSET_TYPE` 为对应资产类型

#### Scenario: 光照按类型拆分

- **WHEN** 取 `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`
- **THEN** 各自 `Data` 只含该光源类型的字段（方向光无 range/cone；点光无 cone；聚光含 inner/outer cone）

### Requirement: 编辑器经扩展模块对接

编辑器 SHALL 通过**编辑器扩展模块**（`AuroraEditorModule : AuroraModule` + `REGISTER_MODULE`，由编辑器模块配置加载）对接 aurora，SHALL NOT 由应用入口显式调用 `AuroraReflection`。扩展模块 `Init` SHALL 走基类 `AuroraModule::Init`（内部调用 `AuroraReflection`）并注册编辑器侧扩展（actor/asset creator、preview）。资产成员 SHALL 走 `Uuid` + `ASSET_TYPE`，以便编辑器 `PropertyUuid` 拾取器无需改动即可工作。

#### Scenario: 编辑器经扩展模块加载 aurora

- **WHEN** 编辑器加载 aurora 编辑器扩展模块
- **THEN** 组件/资产元数据通过组件扩展路径可见，`"Aurora"` 分组组件出现且资产成员可编辑

#### Scenario: 运行时不依赖编辑器扩展

- **WHEN** launcher 运行 `AuroraModule::Init`
- **THEN** 反射/组件注册完成，无需编辑器扩展模块

### Requirement: 图像资产支持 2D / 2DArray / 3D 与版本

`ImageAssetData`（`aurora/adaptor/assets/ImageAsset.h`）SHALL 携带版本号 `version`（`CURRENT_VERSION`）与图像类型 `ImageAssetType`（`TEXTURE_2D` / `TEXTURE_2D_ARRAY` / `TEXTURE_3D` / `TEXTURE_CUBE`），并 SHALL 含 `format`(`PixelFormat`) / `width` / `height` / `depth` / `mipLevels` / `arrayLayers`，以及 `slices`（`ImageSliceHeader{offset,size,mipLevel, layer, depth}`）与 `rawData`（字节载荷）。`Save`/`Load` SHALL 支持上述布局；`Load` SHALL 在 `version != CURRENT_VERSION` 时拒绝加载（保持空数据并记录），SHALL NOT 静默按新布局解析旧数据。

#### Scenario: 2D / 2DArray / 3D / Cube 往返

- **WHEN** 分别以 `TEXTURE_2D` / `TEXTURE_2D_ARRAY` / `TEXTURE_3D` / `TEXTURE_CUBE` 序列化再反序列化
- **THEN** `version` / `type` / `format` / `width` / `height` / `depth` / `mipLevels` / `arrayLayers` / `slices` / `rawData` 一致

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的数据
- **THEN** 拒绝加载（不产生错误尺寸/类型的资源），并记录错误

### Requirement: CreateTextureFromAsset 构建纹理

SHALL 提供 `CounterPtr<Texture> CreateTextureFromAsset(Device *device, const Asset<Texture> &asset)`，按 `ImageAssetType` 映射 `Image::Descriptor` 并创建对应 `Texture` 子类：`TEXTURE_2D` → `Texture2D`（`IMAGE_2D`，`arrayLayers=1`）、`TEXTURE_2D_ARRAY` → `Texture2DArray`（`IMAGE_2D`，`arrayLayers=N`）、`TEXTURE_3D` → `Texture3D`（`IMAGE_3D`，`extent.z=depth`）、`TEXTURE_CUBE` → `TextureCube`（`IMAGE_2D`，`arrayLayers=6`，`viewUsage=CUBE_MAP_COMPATIBLE`）。它 SHALL 遍历 `slices` 以 `ImageUploadRequest{mipLevel, layer, imageExtent}` 上传 `rawData` 的对应区段。

#### Scenario: 按类型创建并上传

- **WHEN** 用 `TEXTURE_2D_ARRAY`（`arrayLayers > 1`）的资产调用 `CreateTextureFromAsset`
- **THEN** 得到 `Texture2DArray`，且每个 slice 按 `mipLevel`/`layer` 上传

#### Scenario: Cubemap 创建

- **WHEN** 用 `TEXTURE_CUBE`（`arrayLayers=6`）的资产调用 `CreateTextureFromAsset`
- **THEN** 得到 `TextureCube`（`viewUsage` 含 `CUBE_MAP_COMPATIBLE`），6 个面按 layer 上传

#### Scenario: Cube layer 数非法

- **WHEN** `TEXTURE_CUBE` 的 `arrayLayers != 6`
- **THEN** 拒绝创建（返回空 `CounterPtr`）

#### Scenario: 无效资产

- **WHEN** 资产版本不匹配或 `device` 为空
- **THEN** 返回空 `CounterPtr`，不崩溃

### Requirement: Mesh 资产材质槽引用与版本

MeshAssetData SHALL 保留 version、materials UUID 槽表与 GetMaterialUuid(materialIndex)，子网格 materialIndex SHALL 按表解析。新产品 SHALL 序列化独立 Mesh 的几何及输入槽契约，SHALL NOT 序列化选定 Skin/Skeleton UUID 或 inverse bind bulk。版本不匹配 SHALL 显式拒绝，SHALL NOT 按新布局读取旧产品。材质槽行为 SHALL 保持兼容。

#### Scenario: 材质槽与版本往返
- **WHEN** 新版 MeshAssetData 带 materials 完成 Save/Load
- **THEN** version 和 materials 顺序 SHALL 一致

#### Scenario: 解析材质索引
- **WHEN** materialIndex 小于 materials.size()
- **THEN** GetMaterialUuid SHALL 返回对应槽 UUID

#### Scenario: 越界索引回退
- **WHEN** 非空槽表的 materialIndex 越界
- **THEN** SHALL 保持既有槽 0 回退并报告警告

#### Scenario: 空槽表
- **WHEN** materials 为空
- **THEN** 子网格 SHALL 解析为无绑定材质且不崩溃

#### Scenario: 实例绑定不进入 Mesh 产品
- **WHEN** 两个组件为相同 Mesh 选择不同 Skin/Skeleton 后保存资产和 World
- **THEN** 各绑定 SHALL 保存在对应组件，Mesh 产品 SHALL 无 Skin/Skeleton 引用

#### Scenario: 版本不匹配
- **WHEN** loader 读取旧 Mesh 产品版本
- **THEN** SHALL 返回 recook 诊断且不产生错误索引或材质资源

### Requirement: StaticMeshComponent 材质槽 0 覆盖

`StaticMeshComponentData.material` SHALL 为可选的**材质槽 0 覆盖**：为空时 SHALL 使用 mesh 资产的 `materials[0]`；非空时 SHALL 仅覆盖槽 0，`materialIndex != 0` 的子网格 SHALL 仍使用资产 `materials[materialIndex]`。该资产引用成员 SHALL 保持 `Uuid` 并以 `SET_ASSET_TYPE(AssetTraits<Material>::ASSET_TYPE)` 标注。

#### Scenario: 空覆盖取资产槽 0

- **WHEN** `material` 为空且 mesh 资产含非空 `materials`
- **THEN** 槽 0 的子网格解析为资产 `materials[0]`

#### Scenario: 非空仅覆盖槽 0

- **WHEN** `material` 非空且 mesh 资产含多个槽
- **THEN** 槽 0 子网格解析为组件 `material`，其余子网格仍解析为资产 `materials[materialIndex]`

#### Scenario: 资产成员带类型元数据

- **WHEN** 取 `StaticMeshComponent` 的材质成员节点
- **THEN** 其 `CommonPropertyKey::ASSET_TYPE` 为 `AuroraMaterial`

### Requirement: Material 资产（typed 属性 + technique + 版本）

MaterialAssetData SHALL persist versioned explicit tagged ShaderTechnique stage/source/entry references, material render state and typed canonical Key/Value properties. Values SHALL use the checked semantic variant, textures SHALL use asset UUIDs and samplers SHALL use typed descriptors; serialization SHALL NOT copy C++ object padding. Complete Material keys SHALL be reconstructed from declaration metadata and Shader-local options. The current payload is version 4; supported legacy version 3 aliases/options SHALL migrate through exact declarations, while unsupported versions and ambiguous mappings SHALL fail with recook/migration diagnostics.

#### Scenario: Typed round trip
- **WHEN** a valid material with explicit programs, values, texture UUIDs and samplers is saved and loaded
- **THEN** tags, stage references, render state and semantic property values remain unchanged

#### Scenario: Legacy alias is ambiguous
- **WHEN** an old key cannot map uniquely to the current complete schema
- **THEN** preparation rejects the migration without guessing a destination

### Requirement: MaterialInstance 资产（父材质 + 覆盖 + 版本）

MaterialInstanceAssetData SHALL persist its supported version, parent Material UUID and typed sparse value/texture/sampler overrides. Unoverridden fields SHALL inherit the parent; explicit equal-to-default writes SHALL still count as overrides. Instance preparation SHALL validate canonical names and types against the parent schema and SHALL NOT allow static Shader-local option changes. Unsupported versions or invalid overrides SHALL fail without publishing a partial instance.

#### Scenario: Sparse override
- **WHEN** an instance overrides one canonical value and texture
- **THEN** those values are retained and all other fields inherit the parent

#### Scenario: Static option override
- **WHEN** an instance attempts to override a Shader-local static option
- **THEN** preparation rejects it explicitly

### Requirement: CreateMaterialFromAsset / CreateMaterialInstanceFromAsset

The existing creation helpers SHALL retain checked tagged programs and typed properties rather than the removed single ShaderRef technique shape. Texture UUIDs SHALL resolve through AssetManager with explicit failure for invalid resources; missing Device or invalid data SHALL return an empty result. Production SceneAssetSource SHALL additionally resolve complete logical Material schema from source or product metadata and freeze named values, explicit selected variants and exact resource dependencies before GPU preparation. Asset conversion SHALL NOT prepack a target's uniform bytes.

#### Scenario: Valid material conversion
- **WHEN** valid typed data is converted with a compatible device
- **THEN** the Material retains its explicit technique programs and property values

#### Scenario: Invalid creation input
- **WHEN** Device is absent or a required asset/value cannot be resolved
- **THEN** conversion returns failure without publishing a partially valid material

### Requirement: Skeleton 资产桥接

`engine/aurora/adaptor` SHALL 提供 `SkeletonAssetData`（`aurora/adaptor/assets/SkeletonAsset.h`，`version = CURRENT_VERSION`，包 `sky::SkeletonData`：`boneData`(`Name` + `parentIndex`) 与 `refPos`(`Transform`)，`Save`/`Load` 逐字段）与 `AssetTraits<sky::Skeleton>`（`DataType = SkeletonAssetData`，`ASSET_TYPE = "AuroraSkeleton"`，`BIN`），并提供 `FetchOrCreateSkeletonByAsset`（asset → `Skeleton::BuildSkeleton`）。`Aurora.Adaptor.Static` SHALL 链接 `Animation`（桥接层唯一的 Skeleton 依赖点）；`AuroraReflection` SHALL 注册 `SkeletonAssetData` 与 `RegisterAssetHandler<sky::Skeleton>()`。`aurora/core` SHALL NOT 定义 `Skeleton`/`Bone`。

#### Scenario: 注册骨架资产

- **WHEN** `AuroraReflection` 完成
- **THEN** `AssetManager` 已注册 `sky::Skeleton` 的 handler，且 `AssetTraits<sky::Skeleton>::ASSET_TYPE == "AuroraSkeleton"`

#### Scenario: 骨架数据往返

- **WHEN** 序列化再反序列化 `SkeletonAssetData`
- **THEN** `version` / `boneData`（name、parentIndex）/ `refPos` 一致

#### Scenario: 构建运行时骨架

- **WHEN** 对一个含非空 `boneData` 的 `Asset<Skeleton>` 调 `FetchOrCreateSkeletonByAsset`
- **THEN** 得到非空 `SkeletonPtr`（`GetNumBones() == boneData.size()`）

### Requirement: Renderer foundation 实现与模块归属

Renderer 接口与实现 SHALL 属于现有 Aurora core，Framework 服务注册与 World/资产对接 SHALL 属于现有 Aurora adaptor，运行时入口 SHALL 复用 Aurora.Render。不得为 Renderer 再建立 core/adaptor/runtime 实现模块体系或 Aurora plugin。仅接口 target SHALL 不链接实现；CPU prepare SHALL 不访问 GPU 或 Framework 可变对象，内部实现头 SHALL 不作为公开 include 传播。

#### Scenario: 宿主只消费接口
- **WHEN** 宿主包含 aurora/renderer 公共接口
- **THEN** 原 include 路径有效，接口 target 不要求链接 Aurora.Core.Static 或 Aurora.Adaptor.Static

#### Scenario: 动态与静态模块装载
- **WHEN** 宿主通过动态或 tagged static 方式初始化 Aurora.Render
- **THEN** 初始化成功后发布同一 Environment 中的 provider，重复 owner 不被覆盖，Shutdown 只撤销自己的注册

#### Scenario: CPU prepare 与内部实现边界
- **WHEN** Renderer 实现编译进 Aurora.Core.Static
- **THEN** scene/prepare/frame 代码仍只消费 CPU 接口数据，graph/resource 内部头不经 target 的公开 include 传播给宿主

### Requirement: 渲染端 UI feature

UI adaptor SHALL 将 owned vertices、indices、draw order、clip、圆角参数、精确图像版本和 logical ViewOutput 引用编码为有界快照。渲染端 SHALL 通过消费方 FrameGraphBuildContext 创建图声明与 lease-owned 上传，不读取可变编辑器 UI 对象。shader 编译归生产者，native 对象创建与退休归 coordinator。

#### Scenario: 透明 UI draw 并行编码
- **WHEN** UI draw 形成多于 encode worker 的任务片段且有多个帧在途
- **THEN** 片段按快照顺序提交，clip、alpha blend 与 LOAD 像素结果保持一致

#### Scenario: 一个 UI 窗口不可用
- **WHEN** 两个 UI 输出采样同一个离屏结果，其中一个 native 输出不能 acquire
- **THEN** 只跳过不可用窗口，共享 producer 和另一输出的采样绘制继续

### Requirement: RenderSceneProxy 命令入口

Aurora adaptor SHALL 提供主线程 RenderSceneProxy，将宿主 scene 生命周期映射为 Renderer IDs 与有序命令。Proxy SHALL 只消费 IRenderer 接口，不访问渲染线程 ECS 或 provider 具体类型。完整组件/资产事件提取 SHALL 由 aurora-scene-bridge 实施并复用该命令入口。

#### Scenario: Scene 创建与关闭
- **WHEN** 宿主创建或关闭 World
- **THEN** Proxy 提交对应 scene 命令并处理完成状态，不跨线程调用 RenderScene 的 Add/Get/DestroyEntity

#### Scenario: Scene 等待退休
- **WHEN** Proxy 已向宿主 packet 追加 Scene 销毁命令
- **THEN** 不再暴露该 Scene 为可写目标，收到匹配 generation 的 SceneDestroyed 或整个 Renderer 已停止后才释放生命周期状态；不匹配的旧回执不能释放新 Scene

### Requirement: 编辑器共享 Renderer 与 UI 快照

编辑器 SHALL 使用同一 IRenderer 接口提交主窗、工具窗和离屏 View 请求。UI 布局、输入、平台事件 SHALL 留在主线程；UI 绘制数据 SHALL 以拥有数据的快照交给渲染端扩展，资源上传/绑定按 FrameSlot 保活。合成 SHALL 显式声明对离屏结果的读取；Renderer 实现 SHALL 不依赖 EditorShell 或在渲染线程调用捕获 shell 状态的 GuiPaintFn。

#### Scenario: 主窗合成双场景视图
- **WHEN** 主线程 UI 快照引用两个离屏 View 的图像
- **THEN** FrameComposer 在同一 Frame RDG 声明其采样依赖，渲染端使用快照完成 UI 绘制而不读取主线程可变 UI 状态

### Requirement: Provider 注册与卸载顺序

宿主 SHALL 经 engine 接口的显式注入或服务注册取得 Renderer。provider SHALL 先于消费者就绪，消费者 SHALL 先停止提交再关闭 provider；模块会话关闭 SHALL 等待渲染线程退出与资源退休，DLL 代码 SHALL 由装载层保持进程常驻。跨 DLL 共享状态 SHALL 使用 sky::Singleton；每份反射数据 SHALL 由唯一 metadata owner 在进程 definition 创建时注册一次，SHALL NOT 使用 Once、once_flag、static 标志或查表跳过掩盖重复入口；静态模块构建 SHALL 提供相同服务解析行为。

#### Scenario: 多模块消费同一服务
- **WHEN** runtime 与 editor 扩展在一个宿主进程解析 Renderer
- **THEN** 得到同一活动实例，重复注册不创建第二线程/设备，会话关闭后撤销服务注册，不留下访问已销毁服务或数据的回调

#### Scenario: 模块初始化失败
- **WHEN** 动态或静态渲染宿主模块初始化失败
- **THEN** 清理部分初始化状态并在模块代码仍可执行时销毁对象，不调用该失败模块的 Start/Tick，也不留下已发布的 Renderer

#### Scenario: 既有模块的 provider-only 启动
- **WHEN** Aurora.Render 以 provider-only 模式加载，消费者创建真实 Renderer 后请求关闭
- **THEN** 模块只注册 provider，不启动旧 device/loop；消费者收到 RendererStopped 并销毁对象后才能关闭 provider 会话，动态与静态入口均可再次初始化并创建新 epoch 的实例，DLL 保持常驻

### Requirement: 编辑器 CPU 纹理与快照版本

UI texture registry SHALL 仅持有有界 CPU 数据与逻辑版本。主线程 SHALL 先完成本帧所有窗口的字形/图集修改，再生成拥有数据的快照；每个 image draw SHALL 使用确切 product/revision/usage/device epoch。无效或未就绪版本 SHALL 跳过该 draw，不以其他版本替换。

#### Scenario: 多窗口共享更新后的字体图集
- **WHEN** 后绘制窗口新增字形导致 atlas revision 更新
- **THEN** 本帧全部 UI 快照引用该最终 revision，其上传沿用 Renderer 的资源和帧预算

#### Scenario: 图集更新或释放
- **WHEN** CPU registry 更新或释放已发布纹理
- **THEN** producer 发出旧版本释放请求；已接纳帧和未决上传继续持有需要的数据，最后借用退出后回收

#### Scenario: 工具窗口关闭
- **WHEN** 主线程收到工具窗口关闭请求
- **THEN** 只停止该输出的新快照并提交 DestroyViewport；收到匹配回执后才解除窗口事件监听和销毁 HWND，其它输出继续提交

### Requirement: Feature 模块代码保活

动态模块 loader SHALL 将成功加载的 DLL 保持到进程退出，不向 Feature 或帧逐层传递代码 lease。模块会话 Shutdown SHALL 撤销服务并收齐任务；Feature 实例及其数据 SHALL 由实际 owner 保持到最后的 CPU、GPU、presentation 借用退出。二进制更新和移除 SHALL 在进程重启后生效。

#### Scenario: Manager 清理后仍有 Feature 借用
- **WHEN** ModuleManager 已 Shutdown/销毁模块对象，而已接受帧仍持有 Feature 实例
- **THEN** Feature 及所需数据保活到帧退休，动态库在最后对象释放后仍保持进程常驻

#### Scenario: Provider 初始化失败
- **WHEN** provider 初始化失败或声明的依赖未能加载
- **THEN** 不初始化依赖它的消费者，宿主返回明确启动失败并清理已初始化模块

### Requirement: 参数化图元生成（core/math）

`GeometryGenerator`（`core/math`）SHALL 提供 `GenerateCube` / `GeneratePlane` / `GenerateSphere` / `GenerateCylinder` / `GenerateCone` / `GenerateCapsule`，各产出 `GeometryStreams`（SoA）：

- `positions`(Vector3) / `normals`(Vector3) / `tangents`(Vector4，xyz 对齐 UV U 方向，w=±1) / `uvs`(Vector2) / `indices`(uint32)。
- 数组长度一致（顶点属性同长度）；索引为三角形列表（CCW）。

生成算法 SHALL NOT 依赖 aurora/rhi（纯 `core/math`）。

#### Scenario: 图元数据合法

- **WHEN** `GenerateSphere(1.0, 16, 32)` 后检查输出
- **THEN** `positions/normals/tangents/uvs` 长度一致（`(rings+1)*(sectors+1)`）；法线单位化、`dot(tangent.xyz, normal) ≈ 0`、`tangent.w ∈ {±1}`、`uv ∈ [0,1]`；`indices` 为 3 的倍数且索引不越界

### Requirement: BuiltinGeometry 装配为 RenderGeometry（aurora）

`BuiltinGeometry::Build(Device*, const sky::GeometryStreams&, IndexType)` SHALL 产出 `CounterPtr<RenderGeometry>`：

- 4 个分离 `VertexBuffer`，语义分别为 `POSITION` / `NORMAL` / `TANGENT` / `UV1`（stride = 对应类型 sizeof），已 `Upload`。
- 1 个 `IndexBuffer`（`IndexType` 匹配），已 `Upload`。
- `RenderGeometry` 的 `GetLocalBounds()` 由 positions 的 min/max 计算。
- `indexType == U16` 且 `vertexCount ≥ 65536` 时返回 null。

#### Scenario: Build 后 stream 语义与 bounds

- **WHEN** `Build(device, GenerateCube(1.0), IndexType::U32)` 后检查返回的 `RenderGeometry`
- **THEN** `GetVertexStreams().size() == 4`，语义依次含 POSITION/NORMAL/TANGENT/UV1；`GetIndexBuffer()->GetIndexType() == U32`；`GetLocalBounds()` 覆盖所有顶点

#### Scenario: U16 越界返回 null

- **WHEN** 一个 `vertexCount ≥ 65536` 的 `GeometryStreams` 用 `IndexType::U16` 调 `Build`
- **THEN** 返回 `CounterPtr<RenderGeometry>(nullptr)`

### Requirement: AuroraRender 是可被 launcher 加载的动态模块

`engine/aurora/adaptor` SHALL 产出 `AuroraRender` SHARED 目标，SHALL 通过 `REGISTER_MODULE` 导出 `StartModule`/`StopModule` 并实现 `sky::IModule`。模块 SHALL 是唯一的渲染模块；legacy `SkyRender` 已随 `legacy-stack-retirement` 删除。

#### Scenario: 模块被 ModuleManager 发现

- **WHEN** launcher 选择 aurora 渲染模块
- **THEN** `ModuleManager` 能加载 `AuroraRender` 并调用其 `Init`

### Requirement: 模块初始化 aurora 设备

AuroraModule::Init SHALL 解析 RHI API 并通过 IRenderer 初始化 provider；Device、FrameSlots 和 command 资源 SHALL 由渲染端持有。初始化 SHALL 返回明确成功/失败结果，失败时不进入 GPU 帧循环；Shutdown SHALL 安全清理已创建部分。

#### Scenario: 指定 RHI API
- **WHEN** 启动参数包含 -r vulkan 或 -r dx12
- **THEN** Renderer 使用对应 API 初始化设备，AuroraModule 不再自行创建另一套设备

#### Scenario: 初始化失败安全
- **WHEN** provider 或设备初始化失败
- **THEN** 错误反馈给宿主，Tick 不发起 GPU 操作，Shutdown 可安全调用

### Requirement: 模块用平台窗口创建 viewport

AuroraModule::Start SHALL 优先从 ISystemNotify 取得主窗口原生 handle，并通过 Renderer 提交窗口描述创建 Viewport。平台窗口创建、事件和销毁 SHALL 留在宿主线程；原生窗口 SHALL 保活至 Renderer 返回销毁确认。

#### Scenario: 创建 swapchain viewport
- **WHEN** 模块 Start 且宿主提供有效窗口 handle
- **THEN** Renderer 创建关联该窗口的 viewport；宿主不直接持有或操作 swapchain

#### Scenario: 原生关闭请求与应用析构
- **WHEN** Windows 宿主收到 WM_CLOSE 或开始退出应用
- **THEN** 渲染中的原生窗口延迟销毁，宿主在渲染退休期间继续处理平台消息；应用在窗口成员析构前完成模块 Shutdown，不能仅把退出放在基类析构函数

#### Scenario: 选择整套宿主模式
- **WHEN** 已迁移平台启动 launcher，或尚未迁移平台启动原宿主
- **THEN** 在初始化时选择唯一一套帧循环；旧宿主不得借用新 Renderer 的 Device 另建 command/frame 资源

### Requirement: Tick 驱动最小出帧闭环

AuroraModule::Tick SHALL 收集状态/命令并提交 FramePacket，由 Renderer 处理 acquire、RDG、submit 与 present。无场景内容的有效窗口 SHALL 支持清屏；有效场景 SHALL 走实际 pipeline。零尺寸 SHALL 不 acquire，单窗口失败 SHALL 不阻断其它输出或命令；成功 acquire 后的失败 SHALL 安全收尾，不直接 Release 一个会等待未 signal semaphore 的 image。

#### Scenario: 出帧并呈现
- **WHEN** viewport 可用且提交 packet
- **THEN** Renderer 完成真实绘制或空场景清屏并呈现，宿主 Tick 不直接编码 GPU 命令

#### Scenario: packet 背压与尺寸变化
- **WHEN** TrySubmit 返回 BUSY 且窗口继续 resize
- **THEN** 保留未接纳 packet 的身份和数据，接纳后再用后续 packet 提交最新尺寸，不修改排队帧快照

#### Scenario: acquire 失败取消单输出
- **WHEN** 一个 viewport Begin/Acquire 失败
- **THEN** 该输出不 present，其余输出继续工作且 lifecycle 命令仍生效

#### Scenario: 零尺寸不 acquire
- **WHEN** surface extent 为零
- **THEN** Renderer 跳过该输出 acquire，恢复尺寸后可正常继续

#### Scenario: 已获取图像的失败收尾
- **WHEN** acquire 成功后建图或提交失败
- **THEN** Renderer 走有效收尾提交或后端错误退休路径，不产生等待未 signal 对象的 Present

### Requirement: launcher 只加载 AuroraRender（移除 legacy render）

`GameApplication` SHALL 只注册并加载 `AuroraRender` 作为渲染模块；the runtime host SHALL NOT link any legacy render target. 启动参数 SHALL NOT 需要渲染模块选择开关。

#### Scenario: 启动即加载 aurora

- **WHEN** the runtime host starts（无渲染选择参数）
- **THEN** `ModuleManager` 加载 `AuroraRender`，且不存在 `SkyRender` 目标

### Requirement: 窗口 handle 由宿主提供

宿主应用 SHALL 通过 `ISystemNotify::GetMainWindowHandle()` 暴露主窗口原生 handle；渲染模块 SHALL 优先从该接口取 handle，SHALL NOT 只依赖 `Platform::GetMainWinHandle()`（Windows SDL 后端返回 nullptr）。

#### Scenario: 用宿主窗口建 swapchain

- **WHEN** 模块 `Start`
- **THEN** 从 `ISystemNotify` 取到主窗口 handle 并初始化 `ClientViewport`

### Requirement: 引擎内置 configs

引擎 SHALL 提供内置 `configs/`（仓库根，`modules_game.json` / `modules_editor.json` / `asset_build_presets.json` / `render_preload_assets.json`），并在构建时部署到可执行文件旁的 `configs/`。`GameApplication` SHALL 在项目 workFs 找不到 `configs/modules_game.json` 时回退到内置配置，使 launcher 无需外部工程即可启动。

#### Scenario: 无工程启动

- **WHEN** 直接运行 launcher 且无 `-p` 工程（或工程无 configs）
- **THEN** 从可执行文件旁的内置 `configs/modules_game.json` 加载模块配置

### Requirement: Launcher 游戏模式从部署的 configs 启动

Outside editor/tool/Android builds the game application SHALL use the executable bundle path as its work file
system, so the builtin `configs/` deployed next to the launcher are found and the launcher boots without a
project directory.

#### Scenario: Boot from deployed configs
- **WHEN** the launcher runs in game mode with `configs/modules_game.json` next to the executable
- **THEN** it SHALL load the config from the bundle path and register its modules without crashing

### Requirement: aurora::LodGroup 核心资源

`aurora/resource/LodGroup.h` SHALL 定义 `sky::aurora::LodGroup : RefObject`，持有有序 `std::vector<LodLevel>`，其中 `LodLevel { float screenSize; CounterPtr<Mesh> mesh; }`。SHALL 提供 `AddLevel`、`GetLevels`、`GetLevelCount`、`GetMesh(uint32_t)`（越界返回 `nullptr`）、`GetBoundingSphere()`（取 level 0 mesh 的 local bounds；无 level 或 mesh 为空时返回空 sphere）与 `SelectLod`。`SelectLod(float screenSize)` SHALL 在有序 levels 上按阈值选择并返回层级索引；`SelectLod(const BoundingBoxSphere &bounds, const Vector3 &viewOrigin, const Matrix4 &proj)` SHALL 先由投影计算 screen size 再复用阈值选择。`LodGroup` SHALL NOT 依赖 framework 或 `Uuid`。

#### Scenario: 阈值选择层级

- **WHEN** levels 的 `screenSize` 阈值为有序值，且给定 screen size 命中某一层
- **THEN** `SelectLod` 返回对应层级索引

#### Scenario: 越界或空网格

- **WHEN** 传入的 level 索引越界，或 level 的 mesh 为空
- **THEN** `GetMesh` 返回 `nullptr`，不崩溃

#### Scenario: 空组包围球

- **WHEN** `LodGroup` 没有任何 level
- **THEN** `GetBoundingSphere` 返回空 sphere，不崩溃

#### Scenario: 投影重载选择

- **WHEN** 用 `BoundingBoxSphere` + `viewOrigin` + `proj` 调用 `SelectLod`
- **THEN** 结果与先用相同参数计算 screen size 再调 `SelectLod(float)` 一致

### Requirement: LodGroup 资产数据与版本

`LodGroupAssetData`（`aurora/adaptor/assets/LodGroupAsset.h`）SHALL 携带 `version`（`CURRENT_VERSION`）与有序 `std::vector<LodGroupLevelData>`，其中 `LodGroupLevelData { float screenSize; Uuid mesh; }`。SHALL 提供 `Save`/`Load`（Bin）；`Load` SHALL 在 `version != CURRENT_VERSION` 时拒绝加载（清空数据并记录），SHALL NOT 静默按新布局解析旧数据。`AssetTraits<sky::aurora::LodGroup>` SHALL 为 `DataType = LodGroupAssetData`、`ASSET_TYPE = "AuroraLodGroup"`、`SERIALIZE_TYPE = BIN`。

#### Scenario: 层级与版本往返

- **WHEN** 以非空 `levels` 与 `version = CURRENT_VERSION` 序列化再反序列化 `LodGroupAssetData`
- **THEN** `version` 与 `levels`（含顺序、`screenSize`、`mesh`）一致

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的 `LodGroupAssetData`
- **THEN** 拒绝加载（保持空 levels），并记录错误

### Requirement: Renderer 对象与模块边界

Aurora SHALL 作为 foundation 保留在 `engine/aurora`，不得注册为 Aurora plugin。系统 SHALL 通过 `IRenderer` 接口提供渲染能力；实现 SHALL 复用既有 `core`，Framework 对接归 `adaptor`，模块入口归 `runtime`，不得再建立 `renderer/{core,adaptor,runtime}` 子模块。公共接口 target SHALL 不链接具体实现。接口 SHALL 只引用句柄、描述、命令和结果数据；不得暴露 World/Actor、具体 plugin 或可变渲染 ECS。活动宿主 SHALL 共享一个 Renderer/Device，GPU 资源缓存 SHALL 按 device 共享。

宿主 SHALL 通过 `IRendererProvider` 创建对象，创建结果 SHALL 为明确错误或非空、已初始化的 Renderer；未实现的运行时或不支持的新后端帧协议不得伪报成功。成功对象 SHALL 提供已解析 backend 和本实例稳定的非零 deviceEpoch；首版 device loss SHALL 进入失败/关闭流程，新实例 SHALL 使用新 epoch。公共接口消费端 SHALL 不链接 provider 实现或 RHI.Static；provider module SHALL 通过共享 Environment 发布服务，不能覆盖另一活动 provider 的注册。

#### Scenario: 游戏与编辑器使用同一接口
- **WHEN** 分别启动游戏宿主或编辑器宿主
- **THEN** 各自通过接口解析 Renderer provider，且每个进程只创建一个活动渲染设备；宿主不链接 provider 的具体实现

#### Scenario: 接口与模块注册独立验证
- **WHEN** 消费端仅链接接口 target，或通过动态/静态模块入口注册并卸载 provider
- **THEN** 公共头可独立编译链接；宿主可解析同一服务，重复注册不覆盖原 owner，卸载只清除自身注册且允许后续重新加载

#### Scenario: 生产创建与阶段贯通
- **WHEN** 支持的平台通过 provider 成功创建 Renderer，并提交包含场景更新、资源载荷和有效 View 输出的 owned packet
- **THEN** 实例贯通 Apply、prepare、资源解析、后端 Compile/Dispatch/Submit 与完成事件；成功创建不能只返回占位对象，关闭会收齐 CPU 借用、GPU 和输出责任

### Requirement: 生产帧组合与事件

成功创建的 Renderer SHALL 复用唯一 CpuRenderCoordinator、ScenePrepareScheduler、ResourceCatalog/ResourceUploader、FrameScheduler 和 DeviceFrameContext；不得额外建立 GPU ring 或对同一个 context 使用多个 completion poller。Create SHALL 等待 coordinator 初始化结果，Device 与 native caches SHALL 在该线程创建和释放，backend module SHALL 保活到 context 及 GPU 引用退出。主线程 SHALL 持有 provider module，收到 RendererStopped 并销毁 Renderer 后才卸载。

FrameBuildContext SHALL 拥有 PreparedFrame、精确资源版本 pins、mesh/material layout、输出配置版本与回读请求；不得跨下一次 Apply 保存 live ECS 访问。CPU prepare SHALL 先于 TryBeginSlot，acquire SHALL 位于 output-pass 准备阶段。资源上传与输出 SHALL 使用同一个 RecordedFrame/Submit，并同时验证 upload receipts 与输出同步责任；只有真实完成才发布 Resident。FrameCompleted SHALL 表示该 packet 的 GPU 工作已完成或没有 GPU 工作，不表示 presentation 已退休。

#### Scenario: 首次候选与预算延后
- **WHEN** packet 包含 owned mesh/material CPU 载荷且尚无兼容 Resident
- **THEN** prepare 可冻结候选 metadata；GPU finalize 仅在该精确版本已 Resident 或本图完整上传可解析时绘制，预算不足只省略该 draw，不能替换已经冻结的 mesh/bounds

#### Scenario: 热更新与共享取消
- **WHEN** 新版本等待上传、多个实体共享 ticket，或最后一个订阅者被删除
- **THEN** 冻结前可选择兼容旧 Resident，已冻结帧保留其 layout/backing；删除一个订阅者不取消共享上传，最后一个订阅者取消会发布状态，在途 GPU 引用仍等待完成；显式重新提供失败版本的 owned payload 会创建新 ticket attempt

#### Scenario: 离屏 resize 排队
- **WHEN** 多个 CPU 帧在等待 GPU slot 期间连续 resize 或销毁同一离屏输出
- **THEN** 每个旧帧继续持有自身输出版本，不读取后续 packet 的尺寸或 image；销毁确认等待此前可能引用该对象的帧，但不等待后续无关帧

#### Scenario: Scene prepare 可恢复失败
- **WHEN** 某 Scene 的派生 bounds/light 或 View projection 计算显式失败
- **THEN** 收齐 jobs/借用并报告 ScenePrepareFailed，暂停受影响 Scene；后续 packet 可处理其它 Scene，受影响 Scene 的有效实体或 View 更新允许重新准备；不能发布失败 system 的部分结果

#### Scenario: GPU frame 错误与设备错误
- **WHEN** 编码失败但设备仍可用，或收到 DeviceLost/不确定提交等终态错误
- **THEN** 前者丢弃本帧并允许后续帧继续；后者拒绝新输入，为已接受包给出结果、收齐 CPU/GPU/输出责任并报告 RendererStopped，下一实例使用不同 epoch

#### Scenario: 有界公共回读
- **WHEN** packet 请求有效 RGBA8 离屏 Viewport 的回读
- **THEN** 图内 copy 保活到 GPU 完成后才发布拥有 bytes 的 ViewportReadback；请求数量和总 bytes 受预算约束，非法输出或格式显式失败，不把 mapped 指针交给主线程

### Requirement: Scene View Viewport 独立生命周期

Renderer SHALL 独立管理 Scene、View 和 Viewport。Scene SHALL 保存 ECS 场景状态；View SHALL 引用一个 Scene，保存独立相机、过滤、可见集合、LOD 和历史状态；Viewport SHALL 描述原生窗口或离屏输出。首版每个场景 View SHALL 关联一个输出，原生窗口 SHALL 可合成多个离屏 View。Scene、Entity、View、Viewport IDs SHALL 包含 generation，销毁确认前不得复用槽位，generation 耗尽时 SHALL 退休槽位。

#### Scenario: 同场景双相机
- **WHEN** 两个 View 引用同一 Scene 并使用不同相机和输出尺寸
- **THEN** mesh/material GPU 资源共享，两套相机、裁剪、LOD、历史与输出尺寸相互独立

#### Scenario: Scene 销毁与迟到更新
- **WHEN** Scene 被销毁且旧 generation 的实体或 View 更新随后到达
- **THEN** 更新被拒绝，相关 View 停止场景绘制并报告状态，原生窗口不被隐式销毁

#### Scenario: CPU 删除与最终退休分离
- **WHEN** CPU 状态已删除，但 coordinator 尚未确认相应 borrowers、GPU 与 presentation 使用结束
- **THEN** 仅产生待退休记录，不发布最终销毁确认；相同槽位的新 generation 创建被跳过并报告，确认后只允许更大的 generation，最大 generation 的槽位永久退休

#### Scenario: 旧 View 不自动绑定新 Scene 或输出
- **WHEN** Scene 或 Viewport 槽位以新 generation 重建
- **THEN** 旧 View 仍保留原引用并暂停，必须经有效 UpdateView 显式重绑后才恢复；零尺寸输出仅暂停自身 View，不影响其它输出

### Requirement: FramePacket 线程与所有权协议

World/UI SHALL 留在宿主主线程，渲染 ECS、Device 与 GPU caches SHALL 由专用渲染线程持有和协调。CPU prepare worker SHALL 仅通过已声明的受限访问借用 ECS/快照；结构变更、GPU 对象/cache 修改、逻辑建图与队列提交 SHALL 由 coordinator 控制。GPU 命令编码 SHALL 由匹配后端的 FrameContext 按计划在 coordinator/encode workers 执行，使用独占编码资源与已发布绑定，不访问 live ECS。主线程 writer SHALL 提交拥有数据的 FramePacket；资产 worker 结果 SHALL 先在主线程归并。包 SHALL 携带顺序号及有序生命周期/更新命令，按 FIFO 处理。载荷 SHALL 按值持有或明确转移不可变所有权；CPU 源数据 SHALL 保活到全部读取/完整 staging 拷贝完成，staging SHALL 保活到 GPU 上传完成。CPU 待处理包上限 SHALL 与 GPU inflight slot 数分开配置。

#### Scenario: 队列已满
- **WHEN** 主线程 TrySubmit 遇到有界队列满
- **THEN** 返回 Busy 且原包仍归调用方；宿主继续处理平台事件后重试，不静默丢弃 create/destroy 命令

#### Scenario: 提交所有权可观察
- **WHEN** TrySubmit 返回 Accepted、Busy 或 Rejected
- **THEN** Accepted 消费并清空调用方包；Busy/Rejected 保留包不变，Accepted 本身不证明 CPU 命令、GPU 或 presentation 已完成

#### Scenario: 通道关闭与消费唤醒
- **WHEN** CPU packet 通道关闭，或 consumer 正等待空队列
- **THEN** 关闭立即拒绝后续提交并唤醒 consumer；已经接收的包仍按 FIFO 交付，只有关闭且排空后才报告通道结束，所有 consumer 退出后才能销毁通道

#### Scenario: 提交序号不因 Busy 消耗
- **WHEN** 非零递增序号的包因队列满而返回 Busy，随后同包重试
- **THEN** Busy 不推进已接收序号，重试可被接收；重复、倒序、零序号和回绕包被拒绝且保留调用方所有权

#### Scenario: CPU 结果消费缓慢
- **WHEN** 已接收包的 CPU 结果尚未被宿主消费，结果交付预算耗尽
- **THEN** 后续 TrySubmit 返回 Busy 并保留包；已接收包已有结果容量，不丢结果、不以宿主读事件作为停机前提，停止事件在全部 packet 结果之后只交付一次

#### Scenario: CPU prepare 失败后的 accepted 包
- **WHEN** CPU prepare 失败而后续包已经被接收，且调用方请求 shutdown
- **THEN** 停止接收新包，收齐当前 kernels/借用，为每个 accepted 包交付执行结果或明确未执行失败；不能直接清空队列或提前报告停止

#### Scenario: CPU 初始化与计划构建失败
- **WHEN** packet/result 容量或 prepare worker 数为零、实例重复初始化，或 prepare builder 返回错误
- **THEN** Init 显式拒绝非法配置且不创建 worker；builder 错误进入当前包结果并保留错误码和诊断，随后按失败 drain 协议关闭；未初始化实例可安全销毁并拒绝提交

#### Scenario: 异步资产过期
- **WHEN** 资产结果的 entity generation 或 asset revision 与当前状态不符
- **THEN** 渲染线程不应用该结果，并安全回收其载荷

### Requirement: CPU 命令预检与持久状态

CPU 场景状态 SHALL 持久保存 ECS 和外部 entity 映射，不因空包或多个 View 重建。命令 SHALL 按包内顺序预检，并在同一 generation 内使用严格递增的非零 Update/Resize revision，Create 基线为零。过期句柄、重复/迟到 revision、退休待确认复用 SHALL 跳过对应命令并报告其索引与原因，不阻断同包其它有效生命周期命令。格式无效或缺少处理器的 payload/extension SHALL 在业务状态修改前拒绝整包。已进入 Apply 的合法序号 SHALL 被消耗，拒绝内容不允许同序号重放。

#### Scenario: 无效包不部分应用
- **WHEN** 一个包前部包含有效删除命令，后部包含非法描述或尚未接入的 payload
- **THEN** 整包报告失败，业务状态保持原状，不返回前部删除的退休记录，后续更大序号的合法包可继续应用

#### Scenario: 迟到更新不撤销同包删除
- **WHEN** 实体删除后，同包还包含旧 generation 的更新，或较新 revision 后又出现旧 revision
- **THEN** 迟到更新按命令索引报告并跳过，已生效删除或新 revision 保留，旧实体不能复活

#### Scenario: 持久状态提供一次 Scene 与独立 View 输入
- **WHEN** 多个有效 View 引用同一 Scene，随后空包或实体增量包到达
- **THEN** registry 与存活 entity 映射保持稳定，本轮 prepare 输入中 Scene 只出现一次，各 View 保留独立相机、过滤与输出描述；删除后所有 View 均不能读到旧实体

### Requirement: 冻结 CPU 输入与读取借用

冻结的 CPU 输入 SHALL 记录最近成功应用的 packet 序号，不得把拒绝内容的序号当作新状态。Scene 输入存储 SHALL 由冻结 lease 保活，上下文及派生读取视图 SHALL 延长该 lease。kernel 不得取得可变 registry；组件读取 SHALL 按 system 已声明的类型、domain 和 owner/generation 校验并返回 const。整个计划的上下文 SHALL 在第一个 kernel 执行前完成校验。CPU 下一包修改、实体/场景删除、退休确认 SHALL 等待所有借用退出，kernel 回调返回或 shutdown 请求本身不代表借用结束。

#### Scenario: 回调已返回但只读输入视图仍被持有
- **WHEN** prepare 回调返回后某 consumer 仍持有从 context 取得的只读输入视图，同时下一包请求更新或删除该实体
- **THEN** 原组件保持有效，下一包及停止完成等待该视图释放；直接被冻结阻挡的 Apply 不消耗包序号，释放后可用同序号继续

#### Scenario: 绑定错误阻止部分 kernel 执行
- **WHEN** 计划后部的 system 声明了错误 owner、未知组件或当前输入层不支持的 Write
- **THEN** Bind 返回无 context 的错误结果，并在运行任何 kernel 前报告绑定失败；kernel 内尝试读取未声明组件返回无效视图并标记该 system 失败

#### Scenario: 拒绝包不改变冻结版本
- **WHEN** 成功应用包 A 后包 B 内容预检失败，随后取得现有状态的冻结视图
- **THEN** 冻结视图仍标记 A 的成功序号，既不发布 B 的部分状态，也不把 B 当作已应用状态

### Requirement: 派生组件的受限写入与成功发布

输入组件 SHALL 仅由 ApplyCommands 写入；派生组件的池和实体项 SHALL 在冻结前创建，kernel SHALL 只通过声明的 WriteView 修改派生值，不取得 registry、任意实体写入入口或结构修改能力。组件指针和稳定行序 SHALL 在第一个 kernel 前绑定，Slice SHALL 只能缩小父视图范围，边界检查 SHALL 防止整数溢出。派生组件的 Read/Write 权限 SHALL 限于该节点的同步 kernel 执行期和执行线程；保存 context/view 不得延长该权限，组件引用不得逃逸迭代回调。只读输入借用仍可延长帧 lease；跨节点消费者 SHALL 通过自己的声明重新取得派生读取权限，或消费独立复制的值。

首个 CPU 对象提取组件 SHALL 区分 value、输入 revision、成功准备 sequence 和 valid，创建时 revision 0 也必须执行首次准备。有效输入更新 SHALL 使缓存失效，迟到更新不得影响缓存；输入未变时可复用成功版本。写入候选 SHALL 在前驱发布后初始化，并只在当前 kernel 完整成功返回后发布；显式 Fail 或非法受限访问 SHALL 丢弃当前节点全部候选，阻止下游消费。该 stamp 仅覆盖已声明的实体输入，不代表资源 metadata、bounds 或 GPU residency 已准备。

#### Scenario: 写后读与连续 writer
- **WHEN** 已编译依赖计划先执行 writer A，再执行 writer B 和 reader
- **THEN** B 的候选以 A 已发布的结果为起点，reader 在相应依赖成功后读取新值；不能在全计划绑定时提前复制旧值导致 B 覆盖 A

#### Scenario: 派生访问权限随 kernel 结束
- **WHEN** 视图被保存后交给另一线程，或在后继节点内再次访问
- **THEN** 派生 Read/Write 返回无效视图或 false 且不访问存储；活跃 scope 的跨线程访问标记生产者失败，已结束 scope 不重新打开权限或改变发布结果；帧重复执行被拒绝

#### Scenario: 范围内准备与版本失效
- **WHEN** kernel 仅写某个 Slice，随后实体更新或以新 generation 重建
- **THEN** 本次仅该范围的数据和 stamp 可发布，空范围无写入，越界或扩大父范围失败；更新令对应缓存失效，新 generation 不继承旧缓存

#### Scenario: 候选完成后 kernel 失败
- **WHEN** kernel 已生成部分或全部候选，但在返回前调用 context.Fail、越权访问或请求越界 Slice
- **THEN** 当前节点不发布任何候选或成功 stamp，迭代返回 false，依赖节点跳过而独立节点可完成；任务结束后保留的输入 ReadView 仍可读取并保活

### Requirement: 每帧场景更新与视图建图

Renderer SHALL 每帧对每个 Scene 串行应用命令，冻结结构后执行一次 CPU prepare DAG；依赖不冲突的 systems SHALL 可并行。该 Scene 的所有 prepare jobs 完成后 SHALL 发布 SceneReady，再分别求值各有效 View；View SHALL 只读已准备的 Scene 并写自身临时数据。coordinator SHALL 在所需 CPU jobs 完成后串行建立一张 Frame RDG，包含场景渲染、离屏消费、UI 合成与输出；由匹配的后端完整编译并在其 DeviceFrameContext 中串行或并行编码，首版使用单 graphics queue 提交。RDG handles SHALL 仅在当帧图中使用。

#### Scenario: 离屏结果参与主窗 UI
- **WHEN** 两个场景 View 的离屏结果在同帧被主窗 UI 采样
- **THEN** 同一 RDG 显式包含场景写入到 UI 读取的依赖，且共享 Scene 的准备只执行一次

#### Scenario: Scene prepare 与多 View 并行
- **WHEN** Bounds 准备和 Light 准备仅共享只读 WorldInfo，之后两个 View 读取同一 Scene
- **THEN** 两个 prepare system 可同时就绪，View jobs 等待 SceneReady 后可并行，各自写入独立 visibility/LOD/draw 数据

### Requirement: Prepare 分块与确定性归并

ScenePrepareScheduler SHALL 支持 system DAG 并行，以及显式 element-wise system 的 entity-range 分块。每个 chunk SHALL 只写自己的实体范围；该 system 的整个组件访问范围 SHALL 保留至所有 chunks 完成，下游不得提前执行。共享 append/reduce SHALL 使用任务私有结果和稳定顺序归并。worker SHALL 不修改 RDG builder、GPU cache 或共享 FrameAllocator 游标。结构命令 SHALL 私有记录后确定性归并，默认在下一帧 ApplyCommands 生效。

分块 SHALL 显式声明 owner 与区间大小，并只为可证明互斥的访问创建 chunk 上下文；不支持的跨 owner 或共享写入 SHALL 在提交前拒绝。所有 chunks SHALL 持独立访问作用域、受限行表和有界私有 scratch。派生候选与成功 stamp SHALL 在完整 system 成功后统一发布；任一 chunk 失败 SHALL 保留原值/stamp、跳过消费者，并收齐其它 chunks。多个失败诊断 SHALL 按稳定 chunk 顺序选择。

#### Scenario: 单 system 多 chunk
- **WHEN** WorldBounds system 被切为多个不重叠实体区间
- **THEN** 各区间可并行写已有 Bounds，消费者等待全部区间完成；chunk 不得借任意实体查询写入其它区间

#### Scenario: 分块候选不能提前发布
- **WHEN** 一部分 chunks 已生成候选，另一个 chunk 仍未退出或最终失败
- **THEN** 在完整 system 成功前不更新实际派生值及 stamp；失败则本 system 的所有候选均不发布，成功则按稳定 chunk/行顺序发布后释放消费者

#### Scenario: 分块私有 scratch 与范围
- **WHEN** 两个 chunks 同时取得 Read/Write 和 scratch，或调用方保存 context 后跨线程/完成后再次请求 scratch
- **THEN** 两个 chunks 的行范围和可写 scratch 不重叠；重新获取 view 不能扩大区间，非法时机不能取得可写 scratch，保留的输入只读视图继续保活存储

#### Scenario: 多个分块写者按完整 system 依赖执行
- **WHEN** 两个写者连续修改相同 Scene 的派生池，后继也被分块
- **THEN** 第二个写者的每个 chunk 在第一个 system 发布后初始化候选；所有读者在第二个 system 完整成功后读取结果

#### Scenario: 稳定输出与延迟结构命令
- **WHEN** worker 完成顺序变化，且某 chunk 请求创建或销毁已注册渲染实体及其固定组件集合
- **THEN** 当前帧池地址和查询集合保持不变；输出与命令按固定 system/chunk 顺序归并，命令在下一帧边界应用

任务输出 SHALL 使用启动前预留的私有容量与非零 channel，复制拥有数据的无指针 byte payload。整个批次成功并收齐任务后 SHALL 按稳定 system/chunk/emission 顺序合并为不借用 ECS 的只读产品；消费者按此顺序 reduce。任何任务失败 SHALL 不发布该批通用输出或 deferred。首版 deferred SHALL 限定已注册渲染实体的创建、更新、销毁；更新/销毁 SHALL 只针对本任务绑定行，并捕获输入 generation/revision，创建 ID SHALL 由调用方提供。任意组件类型的动态增删不属于首版命令集。

#### Scenario: owned 输出与稳定 reduce
- **WHEN** chunks 乱序完成或切换到串行 fallback，kernel 的源 byte buffer 已改变且帧存储已释放
- **THEN** 输出顺序、复制的值和依该顺序计算的 reduce 结果一致；输出可独立保活，不延长 ECS 冻结

#### Scenario: 输出容量与调用范围
- **WHEN** 配置超出批次 records/bytes/deferred 上限、乘法存在溢出风险，或 kernel 超过自身配额
- **THEN** 前者在启动前拒绝；后者显式失败并丢弃批次产品；其它线程或 kernel 完成后不能追加

#### Scenario: deferred 与 packet 一起预检
- **WHEN** 上一成功帧有 deferred，下一 packet 含无效命令，之后又收到有效 packet
- **THEN** 无效包不能部分应用 deferred，pending 保留到有效包；有效包按 deferred 前缀再外部命令应用，整批只消费一次且拒绝重放

#### Scenario: deferred 与外部更新的 revision
- **WHEN** 同一边界多个 deferred 引用相同输入 revision，且外部 packet 又提交合法的新实体输入
- **THEN** deferred 前置条件按边界入口校验；新宿主输入按入口 revision 判定后覆盖候选，本包后续输入仍递增，旧 revision 被跳过

#### Scenario: deferred 的范围与迟到诊断
- **WHEN** kernel 尝试更新/删除其它 chunk 的实体，或 Apply 遇到过期 generation/revision、未退休的复用 ID
- **THEN** 越界 kernel 显式失败；迟到命令跳过且诊断区分 deferred/packet 索引，不复活实体或绕过退休

#### Scenario: 关闭时没有下一 Apply
- **WHEN** 最后一个 accepted packet 的 prepare 生成 deferred，而宿主已关闭入口
- **THEN** coordinator 排空 jobs 和 borrowers，不产生虚构帧；停止结果报告尚未应用的命令数量

### Requirement: Prepare 执行器完成与失败协议

CPU prepare SHALL 复用现有 ThreadPool 实现，通过每帧 JobGroup 跟踪完成。首版 SHALL 使用 Renderer 持有的专用 prepare pool，所有 Scene/View 共用，并限制整个活动计划节点数以满足该池队列的进展预算。worker SHALL 不阻塞等待同池子任务；依赖 SHALL 在提交前建完。空计划 SHALL 立即完成。CPU 下一帧的 live ECS 修改、Scene 销毁、CPU arena reset 和 module 卸载 SHALL 等待引用它们的 jobs 全部退出。任务失败 SHALL 通过显式结果记录并正常释放完成依赖，不使用 throw/try/catch，下游跳过无效结果；失败 Scene SHALL 暂停并报告，恢复需重建，不发布半完成状态。

#### Scenario: 空计划与单 worker
- **WHEN** Scene 没有可处理实体，或用单 worker 执行带依赖及大量逻辑 chunks 的计划
- **THEN** 空计划直接完成；非空计划在预算内批处理并完成，不因零 chunk future、队列饱和或嵌套等待停滞

#### Scenario: system DAG 超过节点预算
- **WHEN** 计划在 hard 任务/scratch 限额内，但 system 或展开后的 chunk/join 节点数超过可安全并行提交的预算
- **THEN** 使用相同依赖顺序的串行执行回退并完成，不向有界队列无上限提交

#### Scenario: 分块预算包含汇合节点且回退保持分块语义
- **WHEN** 单 worker 的 chunks 加 join 恰好达到队列预算，或再多一个节点
- **THEN** 预算内批次可完成，超出时向同一专用 pool 提交一个任务，使用相同分区、scratch 和完整 system 发布逻辑串行执行；coordinator 可订阅和请求取消

#### Scenario: hard 任务与 scratch 限额
- **WHEN** 分块计划超过 hard 节点总数或 scratch 总字节数，或预算计算存在溢出风险
- **THEN** 提交返回明确错误，任何 kernel 都不执行；空行表使用一个空 chunk 和 join，空计划直接完成

#### Scenario: 任务失败时关闭 Scene
- **WHEN** 某 prepare job 失败且宿主同时请求关闭该 Scene
- **THEN** 所有已启动 jobs 完成或跳过后才回收其内存和回调，Scene 不继续发布部分 prepare 结果

#### Scenario: 串行参考执行
- **WHEN** 使用同一已编译 DependencyPlan 在 coordinator 上执行参考路径
- **THEN** kernels 按稳定依赖序运行，显式失败被记录，失败节点的传递消费者被跳过，独立节点仍完成；空计划直接成功

### Requirement: Prepare 逻辑完成与恢复上下文

Prepare JobGroup SHALL 复用 async-threadpool 定义的通用 Completion 与指定 executor 交付协议；coordinator SHALL 能检查状态或订阅 continuation，不依赖阻塞 future.get 判断逻辑完成。全部已启动 kernels 退出、未启动节点被收齐后才能发布最终完成/取消结果。CPU consumers/continuations 的借用 SHALL 独立保活到退出；取消 SHALL 不被当作底层 jobs/GPU 工作已退休的证明。首版 prepare kernel SHALL 一次执行至完成，不挂起，不持有 ECS 访问或任务临时存储跨异步等待；尚未完成的 coroutine SHALL 不被当作普通 TaskNode 已完成的结果。

#### Scenario: worker 完成后恢复协调逻辑
- **WHEN** 最后一个 worker 完成 JobGroup，而后续步骤需要访问 GPU cache 或构建 RDG
- **THEN** 结果只发布一次，后续步骤排队回 render coordinator，不能在该 worker 上直接执行

#### Scenario: 订阅已完成任务或同时取消
- **WHEN** continuation 注册时任务已完成，或取消与完成通知并发
- **THEN** 按完成状态协议恰好交付一次结果或取消结果；捕获对象在相关 jobs 与通知退出前保活，不重复恢复或提前释放

#### Scenario: continuation 交付预算耗尽
- **WHEN** 指定 executor 的订阅交付容量不足
- **THEN** Subscribe 返回 Busy 并保留调用方 callback；Accepted 必须已保留交付容量和 executor/module 生命周期，之后不能因普通 worker 队列满而丢弃通知

#### Scenario: CPU 取消与正常关闭
- **WHEN** host 请求取消当前 prepare，或只请求 graceful shutdown
- **THEN** 取消使未进入的 kernels 跳过，已开始者退出后才发布终态，已有显式失败优先；正常 shutdown 收齐已接受包，不隐式改写成功为取消

#### Scenario: 失败或取消后的 CPU 重建
- **WHEN** 当前 CPU coordinator 收到 Failed/Cancelled，或下一包请求销毁仍有借用的 Scene
- **THEN** 停止继续 prepare 失败状态；等待 kernels、continuations 和 frozen consumers 各自退出后报告已接受包，完整重建状态后才能恢复

#### Scenario: 本帧资源尚未就绪
- **WHEN** 某网格还在 IO、shader 编译或独立资源准备中
- **THEN** 本帧使用声明的跳过/占位策略，不在持有 ECS 访问时等待该外部异步流程；完成结果在后续命令边界校验版本后应用

#### Scenario: 取消时仍有 chunk 和结果消费者
- **WHEN** CancelRequested 到达时仍有 chunk 写入，之后仍有 consumer 使用其结果
- **THEN** 最终 JobGroup 结果等待所有 kernels 退出；ECS 下一轮写入或 arena 回收还等待相关 consumer 退出，提前取消回执不替代这两个条件

### Requirement: FrameSlot 提交与退休

FrameScheduler SHALL 区分 packet sequence、submission serial、frame slot 和 swapchain image index，并使用 DeviceFrameContext 的显式 lease。DeviceFrameContext SHALL 唯一持有 GPU FrameSlot 状态和资源，跟踪 Available、Recording、Submitted、Retired 以及明确的错误分支，并保活 command buffer、UBO、descriptor、上传载荷和 GPU 资源引用直到相应提交完成。scheduler SHALL 持业务 SubmissionRecord，不另建 GPU ring。只有确定提交时 SHALL Reset 提交 fence；空帧 SHALL 返回可用状态。提交失败 SHALL 进入明确的错误/恢复状态并报告，不得等待永远无法完成的 fence。

#### Scenario: 全部窗口最小化后恢复
- **WHEN** 所有窗口输出不可用且无离屏 GPU 工作，随后任一窗口恢复
- **THEN** 期间仍处理生命周期命令，不产生未提交的未 signal fence，恢复后可继续提交

#### Scenario: 多帧在途
- **WHEN** GPU 尚未完成某个 slot 而 CPU 处理后续 packet
- **THEN** 未完成 slot 的常量、descriptor、上传区与资源引用不被覆写或释放

### Requirement: CPU 准备与 FrameContext 编码资源分离

CpuPrepareFrame SHALL 在未取得 GPU slot 时即可持有 CPU 输入、JobGroup 与 task/chunk scratch；资源释放 SHALL 等待对应 jobs/borrowers。FrameBuildContext SHALL 组合 PreparedFrame、DeviceFrameLease、OutputLeases 与单线程 GraphBuildArena。Graph/compiled 非平凡对象和录制回调 SHALL 在 arena reset 前析构或结束，GPU 引用 SHALL 已转交 slot/submission。prepare workers SHALL 不共享 GetFrameAllocator 的可变分配游标。

#### Scenario: GPU slot 尚未空闲
- **WHEN** 上一 GPU 帧仍在途，但下一 CPU packet 可以安全应用
- **THEN** 可以用 CPU 私有 scratch 完成 prepare；GPU allocation/record 等待 lease，不覆写旧 slot

#### Scenario: 有界图构建 arena 与消费者借用
- **WHEN** graph generation 仍被编译、编码或待提交 receipt 使用，调用方请求 reset
- **THEN** 拒绝 reset；最后消费者退出后按逆序析构非平凡对象再回收内存，builder 本身不额外延长冻结图寿命

#### Scenario: 录制完成后释放 CPU 图
- **WHEN** graph/compiled consumers 已退出且提交所需 GPU refs 已移交
- **THEN** 可先析构 CPU graph 和回调再 reset build arena，不必固定等待 GPU 完成；GPU 资源仍由 slot 保活。待提交 receipt 保留图供上传核验，Rejected 保留重试，Accepted/Indeterminate 后清理 CPU receipt，GPU payload 独立按完成或安全 teardown 退休

### Requirement: 复用批次 dispatcher 与独立 prepare pool

ScenePrepareScheduler SHALL 保持 Core 依赖编译与具体执行器分离；批次执行优先适配现有 DeviceFrameDispatcher，提交至 renderer 专用 CPU prepare pool，并补失败收齐和有界 fan-out 进展性。DeviceFrameContext 的 parallelNum/后端 ThreadContext SHALL 仅表示 GPU 编码并行；parallelNum=1 的串行参考模式 SHALL 不妨碍 CPU prepare 多 worker 并行。prepare 与编码 SHALL 使用不同批次、pool 和生命周期，不混用同一个进行中的 dispatcher。

CPU 执行实现 SHALL 只通过 Core 的通用批次执行能力复用 dispatcher，不引入 Renderer core 对 RHI.Static 的依赖。提交 SHALL 预检所有 bindings 并限制绑定总行数；一个 scheduler SHALL 仅有一个进行中的批次，合法计划超过并行节点预算 SHALL 回退串行参考执行。调度接口和 callback SHALL 不抛异常、不使用 try/catch；kernel SHALL 通过 context.Fail 或 checked result 报告失败，标准库异常当前不处理。

#### Scenario: 多 worker prepare 与串行录制
- **WHEN** prepare pool 有多个 worker 而 DeviceFrameContext.parallelNum 为 1
- **THEN** CPU systems/chunks 可按依赖并行，GPU 录制由对应 FrameContext 的串行计划完成，不额外创建编码 worker pool

#### Scenario: 独立 Scene 并行且读者看到发布结果
- **WHEN** 两个独立 Scene 的对象准备同时就绪，多个读者依赖各自 Scene 的派生组件
- **THEN** 两个准备节点可在不同 CPU workers 重叠执行；读者仅在对应 writer 成功发布后执行，双 View 共享同一 Scene 的准备结果

#### Scenario: 并行节点预算边界与串行回退
- **WHEN** 单 worker 或多 worker 的扇出计划恰好达到独占 pool 的节点预算，或再多一个节点
- **THEN** 预算内批次无队列自锁且每节点执行一次；超出预算走串行参考路径，保持相同依赖、结果与成功 stamp

#### Scenario: 失败传播与内部借用释放
- **WHEN** writer 显式 Fail 或访问校验失败，而独立节点仍在执行
- **THEN** writer 候选不发布，依赖消费者跳过，JobGroup 保持 Pending 直到独立节点退出；完成后释放内部 frame borrows，外部保留的输入 ReadView 仍阻挡下一包

#### Scenario: prepare 提交在执行前失败
- **WHEN** 初始化 worker 数为零、计划成环、绑定无效、绑定总行数超预算或已有批次未完成
- **THEN** 对应 Create/Submit 返回明确错误；拒绝的计划不运行任何 kernel，已运行的批次继续收齐

### Requirement: 输出失败与安全关闭

Scheduler SHALL 在逻辑输出 pass 准备、导入输出 attachment 时获取可用输出，并在 Freeze 前将成功结果作为 borrowed lease 导入 RDG。单输出失败 SHALL 不取消其它有效输出和场景命令。已 acquire 后发生失败 SHALL 通过有效的收尾提交或后端错误退休路径处理，不得 present 等待未 signal 的 semaphore。DestroyViewport/Shutdown SHALL 在 GPU 与 presentation 资源退休后确认；宿主 SHALL 在确认后销毁原生窗口，等待期间继续处理平台事件。
Scheduler SHALL 每帧在 BeginViewport 查询 surface 状态和配置，区分有效、暂不可用、需要更新/重建、surface 失效与查询/设备错误，再取得包含 extent、presentation transform 和输出 revision 的准备快照。预检 SHALL 不 acquire；所需更新/重建 SHALL 等待原输出 CPU/GPU/presentation 责任安全退出，成功后才发布可用配置。能力集合无关变化 SHALL 不触发相同配置的重复重建。
Scheduler SHALL 使用 CPU 私有 scratch prepare，再取得可复用 slot 并在输出 pass 准备阶段有界 acquire；GPU 帧分配不得提前覆写未退休 slot。每个 viewport 每帧成功 acquire 一次并供相关 pass 共享；不依赖输出的构图准备可以先执行。等待 SHALL 保证控制事件进展，不越过冻结边界改 ECS；Freeze 前 extent、transform、surface 身份或输出 revision 改变 SHALL 重做受影响 View 或跳过对应输出，后端 Compile SHALL 不 acquire。
可恢复的 acquire 不可用 SHALL 只跳过当前依赖该 backbuffer 的输出 pass，其它有效 pass 与 command buffer SHALL 继续按正常依赖编码和提交。失败输出 SHALL 不伪造 backbuffer、不加入不存在的 acquire/present 同步、不执行 Present；同一输出的使用者 SHALL 分别验证可用性，不得读取未定义内容。输出列表为空 SHALL 不作为取消整帧的条件，DeviceLost 和真实图错误 SHALL 不降级为普通 pass 跳过。

#### Scenario: 单输出 acquire 失败但其它 GPU 工作有效
- **WHEN** 一个输出 acquire 返回 Busy/OutOfDate，图中还有有效输出、离屏绘制或 copy/upload pass
- **THEN** 只省略失败输出 pass；其它工作正常录制并提交，失败输出不 Present、不等待未 signal 的同步，其它输出仍能呈现

#### Scenario: 全部输出 acquire 失败但离屏工作有效
- **WHEN** 全部输出均无法 acquire，图中仍有独立 roots/side effects 保留的离屏或上传工作
- **THEN** 同一 FrameContext 继续编译、编码和提交，完成通知与读回仍有效；只有无剩余 GPU 工作和提交责任时才释放未提交 lease

#### Scenario: 关闭一个在途窗口
- **WHEN** 两个窗口渲染期间关闭其中一个
- **THEN** 另一个窗口继续工作；被关闭窗口停止新 acquire，等待其 GPU 与 presentation 使用退休后返回销毁确认

#### Scenario: prepare 后输出尺寸变化
- **WHEN** View prepare 后 acquire 的实际 extent/generation 与快照不同
- **THEN** 不沿用不匹配的 ViewContext；重做受影响准备或跳过该输出，等待期间仍处理 closing intent

#### Scenario: 同尺寸旋转与在途重建
- **WHEN** BeginViewport 检测到 surface transform 改变，所选 preTransform/输出映射需要更新，但 extent 不变且旧帧仍在途
- **THEN** 使旧输出配置快照失效，停止该配置的新 acquire 并合并最新重建目标；安全退休和更新成功后，View prepare 与输出 pass 使用同一新 transform 快照，不影响其它窗口

#### Scenario: 预检后仅 transform 改变
- **WHEN** BeginViewport 与 acquire 之间 surface transform 改变而尺寸保持不变
- **THEN** Freeze 前的复核仍拒绝旧 ViewContext；已获取输出走安全 Abandon 或明确的重做路径，不能仅因尺寸相同继续提交

#### Scenario: surface 失效与恢复
- **WHEN** surface 查询报告零尺寸、surface lost 或具体查询错误
- **THEN** 分别暂停输出、通知宿主恢复 surface 或传播错误，不以旧 extent 冒充有效；surface 恢复后重新查询配置并创建输出，不反复 resize 失效的 surface

#### Scenario: 获取后复核查询失败
- **WHEN** acquire 已成功，但 Freeze 前的 surface 复核返回查询错误
- **THEN** 保留具体错误并安全 Abandon 该输出，不取消其它有效 pass；普通查询错误允许下帧重新检查。DeviceLost 则封闭设备的新获取和提交，不作为可恢复窗口跳过

#### Scenario: 延迟 acquire 与无可用输出
- **WHEN** CPU prepare 未结束、没有可分配 slot，或所有输出均 Busy/最小化
- **THEN** prepare 不 acquire、不借用 GPU 分区；取得有效 lease 后每个输出只进行有界尝试。没有 GPU 工作只释放未提交 lease，不 reset fence；有获准上传则在同一 context 提交 upload-only 图

#### Scenario: acquired 输出遗漏或构图失败
- **WHEN** 成功 acquire 的图像未成为录制输出，或构图/编码失败
- **THEN** 不提交缺失输出责任的普通帧，也不 present 等待未 signal 的同步；收齐编码和 CPU 图借用后，通过后端未呈现释放或合法收尾提交完成退休

#### Scenario: GPU 完成但 presentation 尚未退休
- **WHEN** graphics completion 已发布，但输出的 presentation fence/资源交还条件仍 Pending
- **THEN** 其它输出可以继续，原输出保持在途记录，不能 resize、复用对应同步或发出窗口销毁确认；空事件容量不丢失关闭通知

#### Scenario: 提交结果不确定
- **WHEN** output recording 的提交不能证明未入队或正常 Accepted
- **THEN** 隔离输出与提交资源，不能直接 present 或正常确认关闭；保活窗口、Device 和 module，按设备错误 teardown 收尾

### Requirement: ECS 增量构建与输入版本一致

Scene SHALL 持久保存 registry，使用稳定且带 generation 的外部 entity 映射，并在帧边界串行增量更新。输入组件、派生组件、Scene/View 帧结果和 GPU slot 数据 SHALL 分离。资源绑定 SHALL 使用精确版本键，dirty stamp SHALL 覆盖派生结果的实际输入版本，并仅在成功生成后推进。metadata、bounds、实际绘制 mesh SHALL 属于同一选定版本；worker SHALL 不持可变 GPU cache 或 RHI 指针。

#### Scenario: 热更新回退保持几何一致
- **WHEN** 新 mesh revision 尚未上传而策略选择旧 resident 版本
- **THEN** 冻结前选定旧版本 metadata/backing，bounds/cull/DrawItem 使用一致版本；不能在晚期解析时更换 mesh 却沿用另一版本的裁剪结果

#### Scenario: deferred 与外部删除同一边界到达
- **WHEN** 上一帧 deferred 命令请求更新实体，而本包随后销毁它
- **THEN** deferred 先应用、本包按序应用，最终实体被删除；后续过期 generation 更新不能使其复活

#### Scenario: 资源版本到达但实体输入未变
- **WHEN** 新的兼容 Resident mesh 在下一次冻结时替代旧版本，而 entity revision 不变
- **THEN** 派生 bounds/LOD 输入重新生成；旧 PreparedFrame 保持原精确 key 和 metadata，新帧绘制只能解析到新选择对应的 backing

#### Scenario: 未就绪资源不阻塞独立光源
- **WHEN** 实体具有有效点光参数而 mesh 或 material 没有可用版本
- **THEN** 按跳过策略不产生 mesh draw，仍可生成对应 View 可见的 owned light 产品，不在 worker 等待上传

#### Scenario: 无效数值不污染派生输出
- **WHEN** 世界矩阵不是有限仿射变换、光参数非法、LOD 阈值非有限或不递增，或合法输入变换后的 bounds 溢出
- **THEN** 输入校验或 prepare 显式失败，不发布该失败批次的半完成 draw/light 产品

### Requirement: FrameGraph 构建产物与扩展边界

Renderer SHALL 在 coordinator 上用冻结的 PreparedFrame、有效输出租约和 UploadBatch 构建一个当帧 RDG。构建 SHALL 先固定资源版本解析和共享上传，再加入逐 View passes 与 FrameComposer，最后 Freeze 并交由后端 Compile/FrameContext Dispatch，收齐 RecordedFrame 后显式 Submit。Frame feature SHALL 通过消费方 engine 接口注册，不能从 callback 直接提交队列或访问宿主/可变 ECS。UI 快照 SHALL 使用 logical texture/ViewOutput ID 及 generation/revision，在本图解析并保留资源 lease。

#### Scenario: UI 快照引用已 resize 的 ViewOutput
- **WHEN** UI 快照产生后对应输出 generation 已变化
- **THEN** 旧引用按声明策略占位或跳过，不使用过期 Image 指针或其它图的 RDG handle

#### Scenario: feature 停用与卸载
- **WHEN** 仍有 jobs、录制回调、continuations 或 GPU leases 引用 feature
- **THEN** 停用阻止未来建图，模块卸载等待所有相关使用退出

#### Scenario: 停用后重新启用
- **WHEN** feature 停用时有尚未建图的排队帧，随后同一 generation 重新启用
- **THEN** 旧 activation 的回调仍不执行，新帧使用新 activation；已有录制和提交继续退休

#### Scenario: 多 View 写同一输出
- **WHEN** 多个 View 按稳定顺序写同一个 viewport，UI 通过其 ViewOutputRef 采样
- **THEN** 引用指向本帧该 viewport 的合成内容；各相机独立图像使用不同 viewport

#### Scenario: GPU 完成但 presentation 未退休
- **WHEN** feature 请求退休，GPU 已完成而相关窗口的 presentation 尚未结束
- **THEN** Feature 实例及帧资源继续保活，OnRetired 和 RenderFeatureRetired 等待最后 presentation 责任退出；DLL 代码保持进程常驻

#### Scenario: feature 声明窗口却不产生绘制
- **WHEN** acquired output 在最终图中没有存活访问/export，其他输出仍有工作
- **THEN** 只放弃该输出，其他 command buffers 正常提交；实际使用的 borrowed output 仍必须匹配 acquire 和提交同步

#### Scenario: 非法 UI 快照与有效命令同包
- **WHEN** owned UI snapshot 的版本、字节数、索引范围或 clip 无效
- **THEN** 在应用 Scene/Viewport 和 feature 命令前拒绝整包，后续有效 packet 仍正常执行

### Requirement: 资源 ticket 版本与订阅

Renderer SHALL 接收拥有数据的 CPU resource payload，按资源身份、content revision、usage/variant 和 device epoch 去重 ticket；订阅 SHALL 单独保存 Scene/Entity generation、bindingRevision 与 request sequence。GPU 创建和状态更新 SHALL 由 coordinator 执行。完整资产 IO/解码仍归宿主资产服务，不进入 prepare kernel 或 Renderer 的 Framework 依赖。

#### Scenario: 多 Scene 共享请求且一个实体销毁
- **WHEN** 两个 Scene 订阅同一资源 ticket，而其中一个 entity 销毁
- **THEN** 只移除该订阅，其它订阅继续；迟到结果校验各自 generation/bindingRevision 后应用，不重复创建相同 GPU 版本

#### Scenario: 失败重试与迟到回调隔离
- **WHEN** 相同 ResourceKey 在失败后以新 request sequence 显式重试，或相同 epoch 下重建资源目录
- **THEN** 新候选获得独立 catalog/attempt 身份；旧 ticket 的交付、失败和发布不影响新候选，旧 subscriber 取消也不能撤销新订阅

#### Scenario: 有界载荷交付
- **WHEN** 目录的 ticket、subscriber 槽位水位或累计 CPU bytes 达到容量，或 payload 的 key/format/bounds 无效
- **THEN** 返回显式错误且不消费 caller payload；取消释放目录所有权，但已有 CPU 借用继续保活源 bytes，上传拥有者独立统计其仍在途的数据

### Requirement: 资源 Resident 与 InGraph 使用资格

资源可用性 SHALL 区分已完成上传且状态已知的 Resident、由当前图内 producer 保证的 InGraph 和 Unavailable。首版 SHALL 用同 graphics queue 的 inline RDG copy 提供 InGraph 使用，消费者声明必要访问/barrier；InGraph SHALL 带 GraphId 且不能跨图使用。提交前不得将 ticket 标成全局 Resident；上传完成、必需范围已初始化及相关状态/ownership 就绪后才能发布。几何、材质依赖和 PSO 兼容性 SHALL 在生成 DrawItem 前检查。

#### Scenario: 同帧新网格上传并绘制
- **WHEN** CpuReady 的 vertex/index payload 被接纳进本图上传预算，且所需 shader/PSO 可用
- **THEN** 本图 copy 先于对应 draw 并有显式访问依赖，CPU 不同步等 GPU 上传；其它图不能复用本图 InGraph 令牌

#### Scenario: 资源延后或上一上传仍在途
- **WHEN** CPU prepare 已选定版本但本图上传预算不足，或该版本在另一 submission 上传尚未完成
- **THEN** 首版跳过或使用已正确准备的回退，不阻塞整个 SceneReady，不在 ResourceFinalize 偷换几何版本

### Requirement: 有界上传与可验证发布

资源调度 SHALL 限制 pending CPU bytes、staging bytes、创建数量和在途上传量。接纳的持久上传 SHALL 为显式图 root，并可在没有窗口输出时形成 upload-only submission。Compile/Record SHALL 只生成 tentative receipts；仅实际录制且提交 Accepted 的 copy 可进入 UploadSubmitted。Resident 发布 SHALL 等待成功完成；Rejected、DeviceLost/不确定错误 SHALL 分别回滚或隔离，不推进错误的资源状态账本。

#### Scenario: 完整 image 载荷的范围验证
- **WHEN** CpuReady 候选声明受支持的 RGBA8 2D image、mip 数及 layer 数
- **THEN** owned bytes 按 layer 再 mip 的顺序紧密保存全部子资源；非法尺寸、溢出或 bytes 数不匹配在消费载荷前拒绝，单个 mip 上传不能代表整个版本就绪

#### Scenario: image 分块接纳与完整版本同图消费
- **WHEN** RGBA8 image 候选大于当前帧、在途或 staging 剩余预算
- **THEN** 按紧密 layer/mip/row 字节前缀切成四字节对齐块并生成持久 copy；允许小于一行，续块复用同一 backing，仅创建时计创建预算；只有本图加上此前已完成内容覆盖完整版本时才返回 InGraph handle

#### Scenario: buffer 分块接纳与完整版本消费
- **WHEN** buffer 候选超过当前帧、inflight 或 lease staging 余量
- **THEN** 按四字节对齐前缀分块，首次创建完整大小 backing，后续复用并以 completedBytes 为目标 offset；仅成功完成的前缀进入图入口，最后一块才允许整版本 InGraph，所有块完成才 Resident

#### Scenario: 部分 buffer 不得绕过发布协议
- **WHEN** 部分 buffer 回到 CPU_READY 等待下一块，或真实 backing 只有创建/Accepted 证据
- **THEN** AdoptResident 拒绝提前发布，目录冻结只选择以前完整的版本；无 native backing 且无上传进度的 CPU-only 验证入口不构成真实 GPU 就绪

#### Scenario: 已提交与已完成进度分离
- **WHEN** 一块被 Prepare、Accepted 或成功 PollCompleted
- **THEN** 分别产生候选、增加 submittedBytes、增加 completedBytes；同一版本最多一个未决块，部分完成不进入 Resident 快照，只有完整版本完成后才发布

#### Scenario: 分块数据与 backing 独立保活
- **WHEN** buffer 或 image 部分版本正在上传或等待下一块
- **THEN** CPU payload 至少到最后一块 Accepted，staging 随本块 lease 退休，backing 跨块保留；两种资源共用数量和逻辑 buffer/texel 字节上限，小块不能绕过整个 backing 预算

#### Scenario: 续传失败或更换 owner
- **WHEN** Rejected、Discard、编码失败、取消或 uploader 被销毁
- **THEN** Rejected 保留原录制及候选，Discard/编码失败收齐后保留以前完成前缀；新 ticket 不继承旧进度；新 owner 必须用保留的 CPU 数据从零重建，或显式失败等待重试，不能把旧前缀用在新 backing 上

#### Scenario: 全部 image 回执才允许发布
- **WHEN** image 上传已完成录制并请求提交
- **THEN** 验证本块全部实际 pass 回执、来源与目标 backing、copy 范围、入口完成前缀及最终状态；遗漏本块 copy、篡改有效前缀/范围或其它存活 pass 覆写该版本时拒绝提交，不登记 UploadSubmitted；成功提交也须等全部版本内容完成才发布

#### Scenario: buffer 与 image 共用事务退休
- **WHEN** 一个批次混合 buffer/image，或 image 上传发生取消、提交拒绝或不确定错误
- **THEN** 共用预算、回滚、提交账本与隔离协议；只有对应 token 成功完成才发布精确版本 backing，目录冻结不把 image 当作 buffer 回退

#### Scenario: 窗口全最小化仍有上传
- **WHEN** 没有可用窗口输出但有预算内的持久资源上传
- **THEN** 可以提交不含 Present 的 RDG，copy 不因无 View consumer 被裁剪；仅真正无 GPU 工作时视为空帧

#### Scenario: 图剔除或提交失败
- **WHEN** 某 upload 未进入 compiled 活跃 passes，或提交已知被拒绝
- **THEN** 不为它发布上传成功/Resident；可重试 CPU 数据保留，未提交的状态增量不提交到账本

#### Scenario: 拒绝后原录制重试
- **WHEN** 已录制上传收到确定未入队的 Rejected，随后用原 RecordedFrame 再次提交
- **THEN** Rejected 不消费 CPU payload 或登记提交 token；后续 Accepted 才进入 UploadSubmitted，仍需成功完成才能成为 Resident

#### Scenario: 不确定提交或完成查询失败
- **WHEN** 上传 Submit 为 Indeterminate，或已 Accepted 的上传收到失败的完成结果
- **THEN** 资源不成为 Resident，停止该上传拥有者继续接纳并保留在途预算/原生引用直到安全退休；Accepted 身份不会被当作成功完成，迟到旧 ticket 不能覆盖新 attempt

#### Scenario: 上传 backing 自动纳管与版本转交
- **WHEN** uploader 首次创建 buffer/image backing 并在后续块续传，或完整版本完成后成为 Resident
- **THEN** 首次取得精确 backing 的账本租约，后续块使用 accepted 初态与 token 核验；完成后租约随版本和冻结快照共享，目录不再持有不意味着尚在使用的快照可以提前退休

#### Scenario: 状态条目满额延后
- **WHEN** 账本容量不足以纳管新上传 backing
- **THEN** 返回明确 STATE_TRACKING 延后原因，保留 CPU payload 与原进度；不驱逐被回退版本、计划或在途提交使用的条目，最后 owner 与 GPU 使用退休后可重新接纳

#### Scenario: 放弃或更换上传 owner 的状态退休
- **WHEN** Prepare 失败、Discard、取消、旧 ticket 或 uploader 析构释放 backing owner
- **THEN** 复用原回滚/失败协议且不从析构直接操作 context；未决计划和 GPU 使用继续阻止回收，终态隔离保留到安全 teardown，部分进度不能绑定到下一 owner 新建的 backing

#### Scenario: 一次完成轮询同时返回成功和设备错误
- **WHEN** PollCompleted 返回的完成数组中既有成功上传又有失败回执
- **THEN** 先处理整批错误，不能因成功项在数组靠前就发布 Resident；真正完成的提交按完成退休，错误提交继续隔离

#### Scenario: 分块上传接续混合 image 状态
- **WHEN** 已完成前缀上传的 backing 在同 context 的中间图改变部分 mip/layer 状态
- **THEN** 下一块导入和回执核验使用完整 accepted image 状态，旧的固定 GENERAL 不得冒充初态；每块仍以统一 GENERAL 收尾，完整版本成功完成后才 Resident，不扩大部分内容有效性

#### Scenario: resolve 写入不得冒充上传版本
- **WHEN** 上传录制回执包含额外 raster resolve 写入同一个待发布 image backing
- **THEN** 回执核验拒绝该候选，不推进提交或 Resident；回滚后仍可用原 CPU payload 重试，普通消费者不能借 resolve 改写不可变上传版本

### Requirement: 资源热更新与取消退休

本帧 SHALL pin 精确资源版本；新版本 SHALL 不原地覆写在途 geometry/descriptor/metadata。旧版本 SHALL 保活至所有 CPU/GPU leases 退休；取消只停止后续工作/发布，不提前释放仍被 jobs 或 GPU 读取的内存。CPU 原始 bytes 与 staging SHALL 使用独立释放条件。

#### Scenario: 上传过程中取消并热更新
- **WHEN** 旧 revision 的上传已提交且新 revision 或取消到达
- **THEN** 旧 staging/backing 仍存活到真实完成；旧结果不覆盖新请求，已有合法 consumer 仍持有其精确版本直到退休

#### Scenario: 冻结回退版本保持 CPU 与 GPU 一致
- **WHEN** desired v8 尚未就绪而同 product/兼容 usageVariant/device epoch 的 v7 已 Resident，或 v8 在 prepare 运行中完成
- **THEN** 当前冻结结果成组选用 v7 metadata/bounds/backing，v8 只影响下一次冻结；不能回退到其它资产、布局族或未来 revision；缓存移除后，旧 CPU 数据和 GPU backing 分别保活至对应消费者退休

### Requirement: 最小真实出图验收

主体 SHALL 使用程序化 indexed mesh、明确的 VS/FS program、真实 PSO、DrawItem 和常量绑定提供可执行验证。验收 SHALL 包括同 Scene 两相机、不同尺寸的窗口/离屏输出、第二个 Scene、实体变换/删除和至少两个 GPU inflight slots。仅 clear 或只检查图结构 SHALL 不构成通过。

#### Scenario: Vulkan 与 DX12 验证
- **WHEN** 最小样例分别运行在 Vulkan 和 DX12
- **THEN** 实际输出或 readback 显示两个相机各自正确的网格结果；连续变换和删除不出现跨视图污染或已删除实体残留

#### Scenario: 未完成平台验证
- **WHEN** Metal surface 线程和 drawable 协议尚未通过平台 probe
- **THEN** 明确记录未迁移状态，保留可选旧入口且不同时启用两套设备宿主，不宣称 Metal 迁移验收完成

### Requirement: 扩展资源精确引用与显式释放

FrameExtensionPayload SHALL 声明有界精确资源 key；Renderer SHALL 校验 device epoch，并在 prepare 前冻结对应候选或 Resident。FrameGraphBuildContext SHALL 仅解析该帧已冻结且就绪的图像。standalone 资源 SHALL 支持幂等释放请求，不能提前回收排队帧的候选数据或破坏未决上传回执。

#### Scenario: UI 图像与上传同图使用
- **WHEN** 某精确版本的完整上传被当前图接纳
- **THEN** 该帧可通过受控 context 解析对应 GraphImageHandle，绘制依赖该图上传且沿用同一 FrameContext

#### Scenario: 释放时旧帧仍在准备或上传
- **WHEN** 新 packet 释放旧图像版本而旧帧仍有 CPU 候选或上传借用
- **THEN** 保留必要版本至这些借用结束，已提交 GPU 图继续通过资源 pins 保活，不影响其它 pass 的提交

### Requirement: Aurora renderer sources build under Clang and GCC

Aurora renderer sources SHALL compile with Clang and GCC as well as MSVC. Member-template calls in dependent contexts SHALL use an explicit non-dependent type or the `template` disambiguator; relying on MSVC's permissive parsing SHALL NOT be treated as portable.

#### Scenario: Clang full build

- **WHEN** the full project is configured and built with AppleClang
- **THEN** the Aurora renderer targets SHALL compile without dependent-template-name errors

#### Scenario: Dependent read call

- **WHEN** a view iterates rows and calls the `Read<T>` member template with a dependent type pack
- **THEN** the call SHALL be written so Clang parses it as a template invocation

#### Scenario: Dependent registry call

- **WHEN** a render command applies component operations through a registry obtained inside a generic lambda
- **THEN** `Add<T>`, `Get<T>`, and `Pool<T>` calls SHALL parse unambiguously under Clang and GCC

#### Scenario: No behavior change

- **WHEN** the portability fix is applied
- **THEN** renderer behavior and public interfaces SHALL remain unchanged

### Requirement: Host explicitly assembles pipeline owners

The host SHALL assemble selected built-in or user features through existing RegisterRenderFeature/IFrameGraphFeature commands and owned Feature instances under process-resident module code. Built-in Scene registration SHALL NOT be a prerequisite or fallback for custom pipelines. Plugins SHALL share the existing producer, Renderer/Device, graph composition and retirement protocols.

#### Scenario: User pipeline replaces built-in scene
- **WHEN** the host selects custom features without registering built-in Scene
- **THEN** those features produce output without constructing built-in Passes or enforcing their layouts

#### Scenario: Owner retires with outstanding work
- **WHEN** an owner retires with accepted compilation, frame or GPU/presentation borrowers
- **THEN** new work stops and existing material/Feature/source/resource/module snapshots remain valid until users complete or drain

### Requirement: Scene techniques match RenderQueue Tags

ScenePass SHALL declare RenderQueues with accepted technique Tags and select each eligible item's matching material technique before queue sorting/execution. Draws SHALL retain the selected technique's actual program, render state and binding inputs. Selection SHALL NOT use the first material technique unconditionally, silently substitute SceneForward.slang, or replace shadow techniques with a universal shader. Material programs SHALL consume compatible exported Global and target-Pass interfaces while retaining their own shading implementation.

#### Scenario: Different material programs in one queue
- **WHEN** Unlit, PBR and user-defined materials each provide a technique matching an OpaquePass queue Tag
- **THEN** their draws enter that queue with their own programs and data while obeying the common Pass resource/output contract

#### Scenario: Opaque and shadow techniques
- **WHEN** a material provides opaque and shadow Tags with different programs
- **THEN** the corresponding queues select their matching programs, preserving material-specific deformation or alpha testing

#### Scenario: Missing or ambiguous Tag
- **WHEN** no technique matches a queue, or multiple techniques match without an explicit selection policy
- **THEN** the non-matching item is excluded, while ambiguous selection reports an explicit error rather than selecting by array order

#### Scenario: Invalid matched technique
- **WHEN** a matched technique has a missing program or incompatible execution/output interface
- **THEN** validation reports an explicit error rather than choosing another technique or an implicit scene shader

#### Scenario: Unused shared declarations
- **WHEN** a material includes a Pass interface without using some declared lighting resources
- **THEN** binding validates actual reflected use without requiring values or algorithms for unused declarations

### Requirement: Fullscreen and compute select techniques explicitly

FullScreenPass and ComputePass SHALL explicitly select their techniques without requiring a scene, RenderQueue Tag match or dummy material. Program descriptions SHALL resolve explicit stage/entry names; fullscreen execution SHALL validate graphics compatibility and compute execution SHALL validate compute-only compatibility. Dispatch dimensions SHALL remain invocation inputs.

#### Scenario: Fullscreen technique without a scene
- **WHEN** a FullScreenPass selects a graphics technique with custom entry names
- **THEN** it renders using those entries and Pass resources without material queue matching or mandatory Batch data

#### Scenario: Compute technique without a material
- **WHEN** a ComputePass selects a compute technique and supplies dispatch dimensions plus Pass inputs/outputs
- **THEN** it dispatches without a scene material, queue Tag or mandatory Batch while retaining existing compute resource/access validation

#### Scenario: Technique stage mismatch
- **WHEN** a selected technique's stages do not satisfy the Pass execution kind
- **THEN** validation rejects it before GPU execution without inferring replacement entries

### Requirement: View shader data belongs to Pass

Resource tiers SHALL use Global set 0, Pass set 1, Material set 2 and Batch set 3. Optional View/camera/projection/viewport shader parameters SHALL belong to the consuming Pass invocation, with no separate View tier. Global values SHALL be common across that pipeline's views for the frame. CPU View organization and scheduling SHALL remain independent of this shader binding choice.

#### Scenario: Several views in one frame
- **WHEN** main, secondary and shadow camera invocations coexist
- **THEN** each uses its own immutable Pass values/resource versions without overwriting another invocation or varying a shared Global camera block

#### Scenario: Pass without View inputs
- **WHEN** a fullscreen, compute or scene Pass does not declare active camera inputs
- **THEN** no camera block or dummy View binding is required

#### Scenario: Feature supplies Pass-wide data
- **WHEN** a Feature produces lighting, a shadow map or IBL used across a Pass
- **THEN** the consumer binds it through the Pass contract rather than classifying it as Batch solely because a Feature produced it

### Requirement: Material and Batch have separate providers

ScenePass SHALL compose Material inputs in set 2 and Feature/draw inputs in Batch set 3. Material SHALL expose at most one cbuffer plus declared resources; Batch SHALL carry transforms, palette and other explicitly owned draw inputs. Each binding/block SHALL have one provider or assembler. Composition SHALL validate all active resources and semantic types against the selected target ABI before atomically replacing draw bindings. Generic rendering SHALL NOT infer GPU offsets from Material CPU storage or require a fixed SceneBatchParams payload.

#### Scenario: Material and Feature contributions
- **WHEN** a technique needs Material constants/textures and Feature transforms/palette
- **THEN** they occupy their respective sets and are published together after validation

#### Scenario: Competing providers
- **WHEN** two providers claim one binding or overlap one block
- **THEN** composition fails and preserves the previous draw

#### Scenario: Optional groups
- **WHEN** fullscreen or compute has no active Material or Batch resources
- **THEN** it needs no dummy bindings or Material object

#### Scenario: Explicit invocation resources
- **WHEN** fullscreen or compute declares active set-2 or set-3 resources
- **THEN** its caller provides the matching inputs regardless of Pass kind

### Requirement: Custom pipelines consume foundation binding mechanisms

Aurora SHALL expose generic binding validation through existing foundation interfaces/implementation targets without forcing consumers to link built-in pipeline implementations. Plugin Pass implementations and declarations SHALL remain plugin-owned. Validation SHALL preserve supported backend/resource boundaries, explicit static/dynamic policy, actual reflection checks and atomic binding replacement. PreparedRaster SHALL retain its geometry/resource-version/snapshot role without owning material layouts or queue-selection policy.

#### Scenario: Standalone custom pipeline target
- **WHEN** a user pipeline consumes shared declarations, bindings and FrameGraph feature interfaces
- **THEN** it builds without linking the built-in Pipeline library or shader compiler to obtain C++ types, and no engine target depends on the plugin

#### Scenario: Unsupported binding capability
- **WHEN** a custom interface requests a resource shape unsupported by its execution path
- **THEN** validation returns an explicit unsupported error without treating declaration export as backend support

### Requirement: Renderer adapter is independent of Skeleton service publication

`AuroraModule` SHALL manage its renderer provider, host and launcher lifecycle without creating, registering, inspecting or withdrawing `IAnimationSkeletonSource`.

#### Scenario: Direct renderer adapter without animation services
- **WHEN** a caller initializes the renderer adapter with no Skeleton source or module manager
- **THEN** Skeleton source availability does not affect provider-only or host initialization

#### Scenario: External Skeleton owner
- **WHEN** an external Skeleton service exists during direct adapter initialization or host failure
- **THEN** the adapter preserves that service through initialization, rollback and shutdown

### Requirement: Runtime assembly owns the Skeleton service session

The existing Aurora runtime entry SHALL compose the Skeleton asset source and renderer adapter with one explicit registration owner and actual source/request/result ownership; defining DLL code SHALL remain process resident without per-object code tokens. Provider-only loading SHALL still publish both required providers without creating a RendererHost.

#### Scenario: Provider-only module session
- **WHEN** the runtime module loads in provider-only mode
- **THEN** renderer and Skeleton providers are available to dependent modules
- **AND** shutdown withdraws the session's providers

#### Scenario: Renderer initialization fails after source publication
- **WHEN** renderer initialization fails after this session registers its Skeleton source
- **THEN** the session withdraws and releases its source
- **AND** services owned by others remain registered

#### Scenario: Duplicate runtime owner
- **WHEN** another runtime instance attempts to publish a Skeleton source over the active owner
- **THEN** initialization fails explicitly
- **AND** cleanup does not withdraw the active owner's services

### Requirement: Service withdrawal and code lifetime are distinct

Runtime shutdown SHALL withdraw its Skeleton interface before draining the renderer host and SHALL release its source after host shutdown. Outstanding requests/results SHALL retain the actual operation, asset and resource owners required after service withdrawal or module manager destruction. The loader SHALL keep defining code process resident without per-object code tokens.

#### Scenario: Request outlives its module session
- **WHEN** a caller retains a Skeleton request or result across runtime shutdown and module manager destruction
- **THEN** the global source interface is withdrawn
- **AND** the retained object's required data remains owned until its last use, while defining code stays resident even after the object is released

#### Scenario: Repeated module session
- **WHEN** initialization/shutdown is repeated or cleanup follows partial failure
- **THEN** the session neither duplicates registration nor unregisters another owner

### Requirement: Project scene input uses the launcher renderer owner

The launcher renderer SHALL consume a registered runtime scene source through an Aurora adaptor interface. The project SHALL provide owned scene/resource data and SHALL NOT submit frames or poll renderer events independently.

#### Scenario: Backpressure
- **WHEN** a packet is busy
- **THEN** the host retries the exact pending packet without appending project commands again.

#### Scenario: Source failure
- **WHEN** a source returns an explicit error
- **THEN** the host abandons that unsubmitted packet and follows its renderer failure and shutdown path.

#### Scenario: Source lifetime
- **WHEN** a project module shuts down
- **THEN** it unregisters its source before destroying project state, and previously queued packets retain no source callbacks or borrowed project storage.

### Requirement: Project arena is loaded through World

The project SHALL load the defaultScene World asset before creating dynamic characters. Static geometry, lights and Physics configuration SHALL come from that asset. Characters SHALL use the World's Physics subsystem, and World::Tick SHALL own its stepping. Static and dynamic visual actors SHALL render through RenderSceneBridge.

#### Scenario: Authored scene changes
- **WHEN** an arena transform, gravity or fixed timestep changes in the World asset
- **THEN** the next launch uses the authored configuration for physics and rendering without editing procedural arena arrays.

#### Scenario: Missing scene
- **WHEN** the descriptor's defaultScene cannot be loaded or built
- **THEN** the project reports an explicit error and exits without substituting a procedural arena.

### Requirement: Original character geometry is imported offline

The project SHALL decode the supported NeoX character mesh offline and retain the original positions, topology, normals and UV data in standard OBJ plus source records. Runtime SHALL load the converted geometry, create dynamic World actors and apply the existing visual cage without coupling the renderer to NeoX parsing or cook dependencies. Prototype materials and unimplemented skeletal animation SHALL be identified explicitly.

#### Scenario: Valid original geometry
- **WHEN** the verified danzai01_lod source is converted
- **THEN** its 4,723 vertices and 8,002 triangles match source GIM bounds within quantization tolerance, all indices are valid and the complete payload is consumed.

#### Scenario: Unsupported or corrupt model
- **WHEN** the converter encounters an unsupported layout or corrupt counts, indices or skin weights
- **THEN** it reports a failure instead of exporting guessed geometry.
- **WHEN** the runtime model is missing or has invalid geometry indices
- **THEN** startup fails explicitly instead of displaying a replacement sphere.

### Requirement: Collision response distinguishes impact and control state

The project SHALL track resolved motion separately from drive input, derive punch strength from the post-physics velocity difference, and distinguish light contact from control loss using recovered thresholds. Strong impacts SHALL suppress movement and jumping until the documented prototype recovery policy completes. Recovery SHALL preserve current velocity without injecting another impulse. Unrecovered native extra-body and recovery semantics SHALL remain explicitly identified as approximations.

#### Scenario: Light contact with held input
- **WHEN** a normal grounded impact has strength 7.7
- **THEN** the character stays controlled, separates immediately, and can steer away or tangentially while contact constrains inward acceleration.
- **AND** ending contact releases this constraint without a fixed impact-protection timer.

#### Scenario: Rolling strong impact
- **WHEN** a rolling impact exceeds the 10.5 stronger-hit threshold
- **THEN** drive input is cleared, the resolved outward velocity survives held inward input, and the HUD shows loss and recovery of control.

#### Scenario: Recovery continuity
- **WHEN** recovery completes or a new impact interrupts recovery
- **THEN** motion remains continuous and the previous-velocity snapshot does not produce a false additional hit.

#### Scenario: Replay and single step
- **WHEN** the user selects replay 1, 2 or 4
- **THEN** the project resets into the player-pair, wall/slide or landing showcase respectively, without exposing rolling through these demonstration controls.
- **WHEN** the paused simulation advances one step
- **THEN** its simulation tick and state timers advance together.

### Requirement: Close-up collision showcase

The project SHALL provide player-pair, wall/slide and landing demonstrations through keys 1, 2 and 4, using authored static World actors and dynamic characters. The pair SHALL face each other and the camera SHALL frame collision detail. Rolling is excluded from these demonstration controls.

#### Scenario: Independent tuning and observable response
- **WHEN** a user edits the reflected lab panel
- **THEN** the runtime SHALL use the same bound settings instance for cage integration, contact restitution, camera and playback
- **AND** gravity/drop height SHALL apply on replay, restitution on the next contact, and other parameters on subsequent simulation/render updates
- **AND** the panel SHALL support undo/redo through CommandService
- **AND** the HUD SHALL display the last pair impact's restitution and actual outgoing speed.

#### Scenario: Soft presentation with limited body separation
- **WHEN** the default automatic pair demonstration makes first contact
- **THEN** its approach drive SHALL stop, the body response SHALL use .08 restitution independently of softer cage parameters, and configurable demonstration braking SHALL control the coast
- **AND** landing SHALL affect cage Y velocity without adding an upward body bounce.

#### Scenario: Directional shadow detail
- **WHEN** the active directional light casts shadows
- **THEN** the existing shadow pass SHALL allocate and rasterize a 2048x2048 D32 map with its current PCF/depth comparison behavior.


#### Scenario: Bounded wall compression
- **WHEN** the extra probe reaches a static vertical showcase wall
- **THEN** its project support constraint SHALL limit inward planar displacement at the configured compression distance
- **AND** tangent motion, vertical movement and moving away SHALL remain permitted
- **AND** the motor SHALL stop reapplying inward acceleration while that contact persists.

#### Scenario: Dynamic characters survive viewport suspension
- **WHEN** a window resize temporarily suspends the viewport while dynamic characters change transforms or mesh versions
- **THEN** renderer resource subscriptions SHALL follow those binding updates even while no output can be prepared
- **AND** restoring the viewport SHALL render the characters with their existing material payloads without resetting the scene or republishing unchanged assets.

### Requirement: Model-specific World2 spring demonstration

The project SHALL provide a separate World2 scene with the imported penguin, goose and peacock bind meshes, defaulting to penguin. Static actors SHALL load through World, and runtime characters SHALL use model-specific spring controls and constraints derived from skin influence regions and skeleton topology.

#### Scenario: Model selection and constraints
- **WHEN** the user presses 5, 6 or 7 in World2
- **THEN** the project SHALL select penguin, goose or peacock and rebuild the current demonstration with that model's capsule and probe dimensions
- **AND** debug display SHALL show structural springs, bend/support links and control particles
- **AND** the existing pair, wall and landing showcases and reflected tuning panel SHALL remain usable.

#### Scenario: Manual jump takes control
- **WHEN** Space or WASD reaches gameplay while automatic replay is active
- **THEN** input SHALL switch to manual control without resetting the character
- **AND** a grounded, controlled character SHALL execute a Space jump
- **AND** focused property editors SHALL retain their keyboard input.

#### Scenario: Peacock abdomen and head probes
- **WHEN** the peacock is selected in World2
- **THEN** it SHALL have an abdomen probe and a separate head probe fitted to the actual head influence region
- **AND** the head probe SHALL follow its simulated control, constrain local head deformation against scene boxes and other-character probes, and avoid duplicate whole-body impulses
- **AND** PROBE and ALL debug modes SHALL distinguish abdomen and head spheres
- **AND** selecting another model SHALL recreate probes using the selected model's profile.

#### Scenario: Original Party model in World2
- **WHEN** the user presses 8 in World2
- **THEN** the original Party mesh SHALL use its seven-point deformation solver, original capsule and abdomen probe
- **AND** pair, wall, landing, live tuning and debug display SHALL remain usable
- **AND** switching back to a bird SHALL restore that bird's rig and probes without stale state.

#### Scenario: Penguin and goose head contact
- **WHEN** the penguin or goose is selected
- **THEN** it SHALL have a separate head probe fitted to its visible head influence region, including a local center offset where needed
- **AND** the offset SHALL be included in following motion and node contact conversion
- **AND** head contact SHALL constrain local rig deformation without an additional whole-body impulse.

#### Scenario: Goose segmented neck contact
- **WHEN** the goose is selected
- **THEN** one middle-neck probe and one head probe SHALL follow their simulated controls and be visible in PROBE/ALL debug modes, with the abdomen probe kept separately
- **AND** scene and other-character contacts SHALL constrain the corresponding local controls without additional main-body impulses
- **AND** same-character probe overlaps SHALL be ignored and model changes SHALL remove all old neck triggers.

#### Scenario: Jelly-like movement response
- **WHEN** the goose accelerates, stops or reverses
- **THEN** its softer neck SHALL visibly lag, swing back and decay within the existing constraints
- **AND** the reflected Rig inertia value SHALL scale acceleration-driven rig response live
- **AND** the rigid-body bounce setting SHALL remain independent and steady translation SHALL not add a scripted perpetual sway.

#### Scenario: Fitted bird wall contact
- **WHEN** a World2 bird presses against the static wall
- **THEN** its abdomen cage SHALL convert contacts and deformation through that model's probe center and radius
- **AND** the existing head/neck probes SHALL bound motor-driven compression using rest-frame support while preserving sliding, escape and jumping
- **AND** positional depenetration SHALL NOT create an additional outward launch velocity
- **AND** the goose default response SHALL retain a small decaying movement lag, with one head sphere and one neck sphere.

### Requirement: Runtime preserves the ECS preparation boundary

The renderer runtime SHALL use the existing CPU coordinator, frozen ECS system program and prepared-product collection as the only scene preparation path. Derived component writes and view extraction SHALL retain declared accesses and producer dependencies. GPU graph stages SHALL consume owned prepared products without accessing mutable ECS storage.

#### Scenario: Scene data is prepared before view extraction
- **WHEN** an accepted packet changes an entity or its selected resource version
- **THEN** the scene producer updates invalidated derived data before dependent view extraction
- **AND** the runtime queues GPU work only from successfully collected prepared products

#### Scenario: A reader outlives prepare kernel execution
- **WHEN** a frozen read view remains borrowed after kernels finish
- **THEN** the coordinator waits for the borrower before the runtime's prepared callback, the next packet mutation or teardown

### Requirement: Packet synchronization precedes ECS freezing

The runtime SHALL synchronize resource subscriptions and output metadata on the coordinator thread before ECS freezing, including bindings changed while their outputs are suspended.

#### Scenario: A moving mesh changes with a suspended viewport
- **WHEN** mesh binding revisions continue to change while output extent is zero
- **THEN** the current mesh and material subscribers remain synchronized
- **AND** rendering resumes with the current bindings when the output becomes available

### Requirement: GPU orchestration preserves cleanup and completion order

The runtime SHALL expose explicit graph and progress stages while preserving uploader-owned completion polling, output acquisition, graph lifetime retention and retirement ordering. Stage failures SHALL propagate explicitly to cleanup that releases graph borrowers before abandoning the output or frame slot.

#### Scenario: Graph construction fails after output acquisition
- **WHEN** a graph stage reports an error
- **THEN** acquired output borrowers and builder references are released before the frame is discarded
- **AND** the accepted packet receives its existing failure notification

#### Scenario: Upload preparation admits no work
- **WHEN** upload preparation returns an invalid batch ID because no upload work was admitted
- **THEN** the runtime does not attempt to discard a nonexistent batch
- **AND** the frame can still render or complete through its normal no-work path

#### Scenario: CPU preparation hands off a successful frame
- **WHEN** a prepared frame is queued for GPU processing
- **THEN** the GPU path owns its final completion notification
- **AND** CPU acknowledgement does not emit a duplicate frame completion
