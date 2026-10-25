## Why

Aurora、Content 和 Animation 的 runtime、cook、editor 共享类型曾由多个入口重复反射，窗口构造也承担注册。Once、已存在跳过和 scratchNode 掩盖了这些重复调用；这违反现行 module-registration spec 和用户要求。

2026-10-25 用户明确批准三个 metadata target 及装载方案：每份数据在进程中只有一个注册 owner；不以幂等守卫容忍重复反射。

## What Changes

- 本 change 新增 Aurora.Metadata、Content.Metadata，复用当前实现，不搬目录。已批准的 Animation.GraphAssets 随完整 add-skeletal-animation-workflow 一起提交，包含 Graph schema/handler 和所有调用方。
- ModuleManager 将进程级 definition 与可重启 runtime session 分开；只有 definition 创建执行 RegisterMeta。
- 类型/组件注册保持严格检查；移除反射入口的 Once、已存在跳过和 scratchNode。
- editor view schema 在模块 metadata 阶段注册；窗口、设置 Bind、读取资产只消费元数据。
- 持久 metadata、builder 和 handler 保留对应代码 lease；重载复用仍存活的 DLL，避免提前 Detach。
- 配置、AssetTool 和测试启动环境明确依赖唯一 owner；同步规则文档。

## Capabilities

### Modified Capabilities

- `module-registration`: 唯一数据 owner、process definition、回调代码生命周期和测试初始化约束。

## Impact

Framework、Aurora、Content、Sandbox 的现有注册入口与测试；game/editor/cook 模块清单。完整动画依赖本 change 的进程 definition/lease 契约，但动画资产、服务与编辑器不进入本批。Shader 变体和材质 schema 保留原行为，仅编译器异步请求的生命周期随本批修正。commit/archive 尚未授权。
