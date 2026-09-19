## ADDED Requirements

### Requirement: adaptor 目录双 target

`engine/aurora/adaptor` SHALL 同时容纳 launcher 动态模块（target `AuroraRender` SHARED；include `aurora/adaptor/AuroraModule.h`）与桥接静态库（target `Aurora.Adaptor` STATIC；`AuroraReflection` + assets + components）。`AuroraRender` SHALL 链接 `Aurora.Adaptor`；`Aurora.Adaptor` SHALL 链接 `Aurora` + `Framework`；`aurora/core` SHALL NOT 依赖 `framework`。

#### Scenario: 两个 target 同目录

- **WHEN** 构建引擎
- **THEN** 同一 `engine/aurora/adaptor` 产出 `AuroraRender`（SHARED）与 `Aurora.Adaptor`（STATIC），且 `aurora/core` 不引入 framework 依赖

### Requirement: AuroraReflection 入口

SHALL 提供 `void sky::aurora::AuroraReflection(sky::SerializationContext *context)`，风格与 `sky::CoreReflection` 一致，负责注册 aurora 类型、asset handler 与组件。它 SHALL 幂等。

#### Scenario: 注册并重复调用安全

- **WHEN** 调用 `AuroraReflection` 一次或多次
- **THEN** aurora 类型/组件可查到，且不触发重复注册断言

### Requirement: 运行时资产层

SHALL 为 `aurora::Mesh` / `Material` / `Texture` 提供 `AssetTraits<T>`（`DataType` + `ASSET_TYPE` + `SERIALIZE_TYPE`）、asset data 结构及 `Json`/`Bin` 存取，并在 `AuroraReflection` 中 `AssetManager::RegisterAssetHandler<T>()`。本 change SHALL 只做运行时加载，SHALL NOT 接离线 `AssetBuilderManager` 构建产物。

#### Scenario: 注册资产处理器

- **WHEN** `AuroraReflection` 完成
- **THEN** `AssetManager` 已注册 `Mesh`/`Material`/`Texture` 的 handler

#### Scenario: 资产数据往返

- **WHEN** 序列化再反序列化对应 asset data
- **THEN** 字段一致

### Requirement: 资产驱动组件

SHALL 提供 `AuroraStaticMeshComponent`（`Data{ Uuid mesh; Uuid material; castShadow; receiveShadow; }`）、`AuroraLightComponent`、`AuroraCameraComponent`，均派生 `ComponentAdaptor<Data>` 并注册到 `ComponentFactory`（分组 `"Aurora"`）。资产引用成员 SHALL 用 `Uuid` 且 SHALL 以 `SET_ASSET_TYPE(AssetTraits<T>::ASSET_TYPE)` 标注。

#### Scenario: 组件注册到组件层

- **WHEN** 查询 `ComponentFactory::GetTypes()` 的 `"Aurora"` 分组
- **THEN** 含 `AuroraStaticMeshComponent` / `AuroraLightComponent` / `AuroraCameraComponent`

#### Scenario: 资产成员带类型元数据

- **WHEN** 取 `AuroraStaticMeshComponent` 的网格成员节点
- **THEN** 其 `CommonPropertyKey::ASSET_TYPE` 为对应资产类型

### Requirement: 编辑器经扩展模块对接

编辑器 SHALL 通过**编辑器扩展模块**（`AuroraEditorModule : AuroraModule` + `REGISTER_MODULE`，由编辑器模块配置加载）对接 aurora，SHALL NOT 由应用入口显式调用 `AuroraReflection`。扩展模块 `Init` SHALL 走基类 `AuroraModule::Init`（内部调用 `AuroraReflection`）并注册编辑器侧扩展（actor/asset creator、preview）。资产成员 SHALL 走 `Uuid` + `ASSET_TYPE`，以便编辑器 `PropertyUuid` 拾取器无需改动即可工作。

#### Scenario: 编辑器经扩展模块加载 aurora

- **WHEN** 编辑器加载 aurora 编辑器扩展模块
- **THEN** 组件/资产元数据通过组件扩展路径可见，`"Aurora"` 分组组件出现且资产成员可编辑

#### Scenario: 运行时不依赖编辑器扩展

- **WHEN** launcher 运行 `AuroraModule::Init`
- **THEN** 反射/组件注册完成，无需编辑器扩展模块
