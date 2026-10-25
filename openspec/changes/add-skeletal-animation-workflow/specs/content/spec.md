## MODIFIED Requirements

### Requirement: 动画资产（AnimationClip / Animation graph）
Content Adaptor SHALL 保留 AnimationClipAssetData、AssetTraits<AnimationClip>、Clip handler、反射和 CreateAnimationClipFromAsset。Clip 产品 SHALL 保存版本、name/frameRate、权威 sourceFrameCount、Skeleton UUID、通道 header/bulk，并校验格式与通道。ContentReflection SHALL 只注册其拥有的 Clip 资产，不再注册空 AnimationAssetData 或 AnimationGraph handler。Graph 的 schema/handler/反射 SHALL 唯一归 animation plugin；原资产类型字符串 AnimationGraph 与源 UUID SHALL 保持。

#### Scenario: 动画资产往返
- **WHEN** 一个有效新版本 Clip 产品完成 Save/Load
- **THEN** name/frameRate/sourceFrameCount/Skeleton UUID 与通道数据 SHALL 保持一致，压缩 SHALL 不改变源时长

#### Scenario: 版本不匹配
- **WHEN** 加载不支持的 Clip 产品版本
- **THEN** loader SHALL 返回格式/recook 诊断，SHALL NOT 按新布局读取旧 payload

#### Scenario: Graph owner 唯一
- **WHEN** Content.Module 与 Animation.World 加载
- **THEN** Content SHALL 仅提供 Clip handler，AnimationGraph SHALL 由 animation plugin 唯一注册

### Requirement: Animation builder（.clip / .graph）
Content 的 AnimationBuilder SHALL 仅声明 .clip，QueryType SHALL 返回 AnimationClip 资产类型。Request SHALL 读取支持的 Clip 源版本，验证压缩前长度/原始通道、压缩后保持 sourceFrameCount，并登记 Skeleton 产品 UUID 与其真实 owner source 依赖。产品发布 SHALL 使用显式结果，失败 SHALL 不被 FinishProduct 报告为成功。Content.Builder.Module SHALL 注册 Clip builder；.graph SHALL 唯一由 Animation.Builder 提供。Content.Module SHALL 不注册任何 builder。

#### Scenario: .clip 产出 AnimationClip
- **WHEN** 构建有效 .clip 源
- **THEN** SHALL 输出保持原始长度的压缩 Clip 并登记 Skeleton 依赖，包括 generated Skeleton 的 owner source

#### Scenario: 注册 builder
- **WHEN** Content.Builder.Module 初始化但 Animation.Builder 未加载
- **THEN** .clip SHALL 可构建，Content SHALL 不提供 .graph 的占位成功路径

#### Scenario: Graph plugin 接管
- **WHEN** Animation.Builder 随工具模块加载
- **THEN** .graph SHALL 由其唯一处理，卸载或缺失时 SHALL 明确不可用而非退回旧空 Graph


### Requirement: Content 模块与 adaptor / builder 分层

Content SHALL retain its existing adaptor, offline builder and runtime targets. Content.Adaptor SHALL provide Clip schema/conversion and the Clip source implementation using Animation and Framework interfaces. Content.Metadata SHALL own ContentReflection and persistent Clip handlers once per process definition. Content.Builder.Module SHALL depend on metadata and register Clip builders through RegisterMeta; it SHALL NOT depend on the runtime service. Content.Module SHALL depend on metadata and publish/revoke IAnimationClipSource in Init/Shutdown without registering reflection again. Graph schema/handlers/builders SHALL belong to the animation plugin. Animation core SHALL remain independent of rendering and Framework implementation.

#### Scenario: Offline cook without runtime service
- **WHEN** Content.Metadata and Content.Builder.Module load without Content.Module
- **THEN** Clip schema, handler and builder are available without starting a runtime Clip provider

#### Scenario: Runtime session restart
- **WHEN** Content.Module shuts down and initializes again
- **THEN** its service is revoked and republished while the same metadata definition remains registered
