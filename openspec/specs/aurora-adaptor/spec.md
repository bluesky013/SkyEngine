# aurora-adaptor Specification

## Purpose
TBD - created by archiving change aurora-adaptor. Update Purpose after archive.
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

宿主 SHALL 经 engine 接口的显式注入或服务注册取得 Renderer。provider SHALL 先于消费者就绪，消费者 SHALL 先停止提交再关闭 provider；模块卸载 SHALL 等待渲染线程退出与资源退休。跨 DLL 共享状态 SHALL 使用 sky::Singleton；每份反射数据 SHALL 由唯一 metadata owner 在进程 definition 创建时注册一次，SHALL NOT 使用 Once、once_flag、static 标志或查表跳过掩盖重复入口；静态模块构建 SHALL 提供相同服务解析行为。

#### Scenario: 多模块消费同一服务
- **WHEN** runtime 与 editor 扩展在一个宿主进程解析 Renderer
- **THEN** 得到同一活动实例，重复注册不创建第二线程/设备，卸载后不留下指向已卸载代码的回调

#### Scenario: 模块初始化失败
- **WHEN** 动态或静态渲染宿主模块初始化失败
- **THEN** 清理部分初始化状态并在模块代码仍可执行时销毁对象，不调用该失败模块的 Start/Tick，也不留下已发布的 Renderer

#### Scenario: 既有模块的 provider-only 启动
- **WHEN** Aurora.Render 以 provider-only 模式加载，消费者创建真实 Renderer 后请求关闭
- **THEN** 模块只注册 provider，不启动旧 device/loop；消费者收到 RendererStopped 并销毁对象后才能卸载，动态与静态入口均可再次加载并创建新 epoch 的实例

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

动态模块 loader SHALL 为 feature 提供真实代码 lease，并使依赖代码至少保活到消费者代码释放。模块逻辑 Shutdown 与代码卸载 SHALL 分离；最后的 feature、CPU、GPU、presentation 借用退出前不得卸载定义其回调的模块。

#### Scenario: Manager 清理后仍有代码 lease
- **WHEN** ModuleManager 已 Shutdown/销毁模块对象，而外部仍持有有效 lease
- **THEN** 动态库保持加载；释放最后 lease 后才执行模块 detach 与 native unload

#### Scenario: Provider 初始化失败
- **WHEN** provider 初始化失败或声明的依赖未能加载
- **THEN** 不初始化依赖它的消费者，宿主返回明确启动失败并清理已初始化模块
