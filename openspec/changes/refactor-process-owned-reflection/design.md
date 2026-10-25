## Context and approval

用户于 2026-10-25 回复“按此方案实施”，批准新增 target、装载入口和下述 definition/session 拆分。Aurora 仍是 foundation，metadata 入口位于 engine/aurora/runtime；没有新增业务 plugin 或目录迁移。

## Ownership

| 元数据 | 唯一 production owner | 消费方 |
| --- | --- | --- |
| Aurora schema、资产 handler、组件与材质反射 | Aurora.Metadata::RegisterMeta | Render、Cook、Render.Editor |
| Content schema 和 handler | Content.Metadata::RegisterMeta | Module、Builder.Module、Views |
| Aurora/Content asset summary 类型 | 各自 editor module::RegisterMeta | 可反复挂载的 view provider |
| ImageCookSettings、各 builder 实例 | 所属 builder module::RegisterMeta | cook 和设置面板 |
| EditorSettings、ViewportControlSettings、Reflection Demo schema | SandboxModule::RegisterMeta | 编辑器 session 与窗口 |
| ExportPreset schema | SerializationContext 构造 | preset 加载、保存、面板 |

每个反射函数保持单次调用契约；不自行查询类型是否存在、不设 Once/once_flag/static bool、不向临时 TypeNode 写入重复结果。SerializationContext 对 UUID 和注册名同时保留严格断言。断言针对错误的程序注册图，不处理外部资产错误。

## Module definition and runtime session

Environment 中共享的 ModuleDefinitions 目录持有进程 definition。definition 创建时执行 RegisterMeta，并保存实际持久 metadata 所需要的 code lease。新的 ModuleManager 或失败 Init 重试解析已有 definition，只创建新的 runtime session；session 只负责 Init/Start/Tick/Shutdown。类型数据的 owner 由依赖图决定，目录不能补救两个不同模块同时注册同一类型的错误。

definition 记录 DLL 的 weak lease。已有 metadata 或外部请求仍持有 DLL 时，新 session 复用同一个装载控制块，只有最后一个 lease 释放时才 StopModule/Detach。没有持久 metadata 的模块不被 definition 强持有，因此 ShaderCompiler 仍在最后一个异步编译结果释放后卸载。

RegisterMeta 期间建立代码作用域。反射类型、asset handler、builder 插入时取得 lease；definition、SerializationContext 和回调容器在所持数据销毁之后释放代码。此处的作用域状态只管理 lease，不决定是否反射；重复类型依旧立即失败。静态模块不需要 DLL lease。

进程 metadata 不随 runtime Shutdown 销毁；AssetManager/SerializationContext 等进程服务的 Destroy 属于最终进程清理，不能在 session 之间重建。运行时 provider 和 view/action 注册仍在 Init/Shutdown 对称挂载/撤销。

## Loading and tests

game/editor 清单、模板、无配置 editor fallback 和 AssetTool 明确声明 metadata 依赖。cook 不通过启动 renderer 或 World 获取 schema。Animation.GraphAssets 与 buildOption 开关随完整动画 change 引入。

单元测试在 executable main 或 gtest Environment::SetUp 注册本 executable 拥有的 fixture 类型；测试用例、getter、builder 构造和窗口创建不能反射。NetworkWorld 与 Bullet 的 World/组件 fixture 遵循同一入口契约。真实模块测试通过实际 metadata DLL 装载；静态 renderer contract fixture 有独立 metadata owner。

## Validation

必须覆盖：静态模块 Init 失败后重试；不同 manager 重用 definition；动态 cook/runtime/editor 同进程重复装载；连续创建材质/设置/Reflection Demo 视图；持久 builder 身份不变；卸载后 MakeAny/handler 回调仍可调用；无 metadata 的 shader compiler 正常卸载。Windows 构建和测试记录见 tasks；未验证平台不能标为完成。

## Commit boundary

用户于 2026-10-26 明确要求剩余材质/Shader 与动画共两个提交。本 change 随前置材质/Shader 提交迁移现有 Aurora/Content 元数据 owner、Framework definition/session、持久回调与异步编译器代码保活、现有编辑器及测试初始化。Content 现有占位 Graph 资产在此快照仍归 Content.Metadata，后续完整动画批次一次性删除旧 handler 并引入 Animation.GraphAssets；任何中间快照都不能同时存在两个 owner。

Skeleton/Skin 产品、Clip provider、Graph/World/editor、角色预览与打包均归后续完整动画提交。前置提交同时交付 Shader 的 V/P/S、四组资源、自动材质 schema 和材质所需通用文档/预览宿主。SKINNED 输入声明、shader deformation helper 与 palette 绑定布局属于 Shader 契约；实际 Skin 资产、姿态/边界快照和 Scene palette 上传随动画接入。不会用 Once、查询类型或临时反射入口拼接提交。

## Existing Clip bridge compatibility

独立 Content 回归发现已提交采样契约的消费方遗漏：旧版 Clip 转换器把 key 数量写入权威帧数，使 `{0,10}` 的稀疏轨道变成 2 帧。本批对仍使用旧产品格式的中间快照做最小兼容处理：不伪造 sourceFrameCount，由已验证轨道时间推导长度；检查 AddChannel/Validate 的显式失败并保持对象清理。现有 ClipBulkRoundTrip 的 11 帧断言不修改。完整动画批次仍负责新产品版本和权威 sourceFrameCount。
