## 1. adaptor 目录双 target

- [x] 1.1 `engine/aurora/adaptor` 保持单一目录，新增静态库 `Aurora.Adaptor`（reflection + assets + components）
- [x] 1.2 `AuroraRender`（SHARED）链接 `Aurora.Adaptor`，两个 target 同目录
- [x] 1.3 `aurora/core` 不引入 framework 依赖

## 2. AuroraReflection 入口与场景类型

- [x] 2.1 `void sky::aurora::AuroraReflection(SerializationContext*)`（幂等）
- [x] 2.2 注册支撑/场景类型：`AABB`、`Bounds`、`WorldInfo`、`LightType`(enum)、`Light`
- [x] 2.3 `AuroraModule::Init` 调用 `AuroraReflection(SerializationContext::Get())`

## 3. 运行时资产层

- [x] 3.1 `AssetTraits<aurora::Mesh>` + `MeshAssetData`（顶点/索引/子网格/bounds）+ Bin Save/Load
- [x] 3.2 `AssetTraits<aurora::Material>` + `MaterialAssetData` + Bin Save/Load
- [x] 3.3 `AssetTraits<aurora::Texture>` + `ImageAssetData` + Bin Save/Load
- [x] 3.4 `AssetManager::RegisterAssetHandler<Mesh/Material/Texture>()`

## 4. 资产驱动组件

- [x] 4.1 `StaticMeshComponent`（`Uuid` mesh/material + `SingleAssetHolder` + `IAssetReadyNotifier`）
- [x] 4.2 光照按类型拆分：`DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`（各自 `Data` 只含该类型字段）
- [x] 4.3 `CameraComponent`（fov/near/far 访问器）
- [x] 4.4 资产成员 `Uuid` + `SET_ASSET_TYPE`；注册到 `ComponentFactory` 组 `"Aurora"`

## 5. 验证

- [x] 5.1 构建 `Aurora.Adaptor` / `AuroraRender` / `Launcher` 成功
- [x] 5.2 `-r dx12` 启动：AuroraReflection 在 `AuroraModule::Init` 运行、无重复注册断言、主循环无崩溃
- [x] 5.3 `openspec validate aurora-adaptor --strict` 通过

## 6. 编辑器扩展模块

- [x] 6.1 `engine/aurora/editor` → `AuroraRender.Editor`（SHARED）
- [x] 6.2 `AuroraEditorModule : AuroraModule` + `REGISTER_MODULE`，`Init` 走基类（内部 `AuroraReflection`）
- [x] 6.3 `AuroraModule` 移入静态库 `Aurora.Adaptor`（对齐 legacy `RenderAdaptor`），`AuroraRender` dll 仅 `REGISTER_MODULE`

## 待办（后续，不在本 change 范围）

- 编辑器模块配置把 aurora 编辑器扩展模块接入（`engine/configs/modules_editor.json`），并协调与 legacy `SkyRender.Editor` 的共存/替换
- 编辑器侧扩展内容（actor/asset creator、asset preview）注册
- `AssetHandler` 的 `DataType` → GPU resource 构建（`Mesh`/`Material`/`Texture` 需 device），当前组件只跟踪 asset handle
- `SkinnedMesh`/`Skin`/`Skeleton` 资产与 animation→Skin 桥接
- `AuroraReflection` 单测（类型已注册、组件分组、JSON 往返）
