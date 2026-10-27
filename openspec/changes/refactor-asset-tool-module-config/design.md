## Context

当前问题源于清单和解析逻辑分散，而不是 Jolt 或无界面工具需要特殊装载方式：

| 入口 | 当前行为 | 问题 |
|---|---|---|
| AssetTool | C++ 列举 builder；Animation/Jolt 使用专用编译宏；audit 单独注册 Compression | 宿主认识每个插件，宏不保证 DLL 构建/部署 |
| GameApplication | 从项目/内置配置解析 modules，并处理项目模块、game 设置及基础模块 | JSON 模块解析与启动策略混合 |
| EditorApplication | 从内置 modules_editor.json 读取模块，有缺省启动策略 | 模块解析与 Game 重复，字段错误处理不一致 |
| ToolApplicationBase | 读取旧 `/config/modules_tool.json` | 路径及错误语义不同，不宜直接作为 AssetTool 的新基类 |
| ModuleManager | 注册依赖、加载、metadata owner 与会话生命周期 | 已能报告加载失败，不应重新实现这批能力 |

`Application::Init()` 的顺序为 ParseStartArgs → LoadConfigs → PreInit → LoadModules。因此不能要求模块配置读取依赖 PreInit 才挂载的资产源目录。

## Goals / Non-Goals

- 宿主代码不维护业务 builder 名单；增减 builder 由配置及其已有 target 声明表达。
- 共用解析和诊断，保留各宿主自己的路径选择与启动策略。
- 单独构建/部署 AssetTool 能得到所选功能所需的模块与配置。
- audit 仍不挂载源资产、不启用 builder、不回退 cook。
- 不引入新 core、模块层级、配置类继承树、清单 include 语言或扫描目录自动发现插件。
- 不调整未触及的 XR/旧 ToolApplicationBase 宿主，也不实现外部插件安装/打包系统。

## Decisions

### 1. 轻量共用解析

在现有 Framework application 目录提取模块清单解析函数；输入为已经读取的 JSON 内容，输出使用已有 `ModuleInfo` 列表和明确错误。解析器不打开文件、不调用 LoadModules、不创建新的进程单例；buildOption 查询复用 ModuleManager 的已生成构建选项表。

完整清单通过校验后，宿主才逐项 RegisterModule，避免前半部分已注册、后半部分解析失败。检查根结构、modules 数组、非空 name、字符串 dependencies、重复条目、未知/错误类型的 buildOption；对大小和条目数设明确上限。合法且为 false 的 buildOption 表示未选择该模块，不尝试加载其 DLL。

依赖仍使用现有 ModuleInfo 字符串列表，允许现有的隐式 metadata 依赖节点；不得强制每个依赖都增加一个重复顶层条目。若启用模块依赖配置中明确禁用的模块，报告冲突，不通过隐式依赖偷偷重启它。依赖环需在装载前报告；复用现有图实现，不再创建第二套注册表。

Game 保留项目模块、配置优先级与 game 字段处理，Editor 保留内置路径与既有缺省策略；这里只替换其模块数组解析。旧宿主的回退行为不得因抽取解析器而悄悄改变。

### 2. 配置选择归宿主，AssetTool 清单归配置

| 模式 | 配置 | 选择规则 |
|---|---|---|
| IPC worker / cook-all / migrate | `configs/modules_asset_tool.json` | 从已解析的显式 engine 根读取；未指定 engine 时，仅在该模式原有参数契约允许的情况下使用 bundle 根 |
| audit-products | `configs/modules_asset_audit.json` | 只读可执行部署的 bundle 配置，不依赖 project/engine 源目录 |
| Game / Editor | 现有配置文件 | 保留原路径与优先级 |

AssetTool 所选配置缺失或损坏即启动失败，不静默使用另一份清单，也不回退到 C++ 模块列表。初期不新增项目级 AssetTool 清单覆盖规则；未来若支持，需要单独明确构建与部署契约。

示例条目使用现有格式：

```json
{
  "modules": [
    {"name": "Aurora.Cook", "dependencies": ["Aurora.Metadata"]},
    {"name": "JoltPhysics.Builder", "buildOption": "SKY_BUILD_JOLT", "dependencies": []}
  ]
}
```

该片段不是最终完整清单。实施时按实际 target 与功能开关补齐 Audio、Navigation、Terrain、Content、Animation 和必要 metadata/产品读取依赖；不能从模块名猜测构建条件。Jolt Builder 使用包含它的资产 change，Jolt runtime adaptor 不因 cook 配置而加载。

### 3. 构建、静态注册和部署消费同一份清单

复用现有 CMake JSON、dependency helper 和部署函数，在相关 target 声明完毕后绑定 AssetTool 的构建依赖。两份配置及其生效依赖闭包是输入，文件变化触发重新配置。

- 动态构建：增加 build-only 依赖，不通过链接 builder DLL 代替运行时加载；生成配置与模块二进制一起部署到工具 bundle。
- 已禁用的可选模块不成为 target/DLL 依赖；仓库内启用条目缺少 target 时明确诊断，不能当作可选模块静默删掉。
- 静态构建：收集 cook/audit 所需工厂的并集，但只激活当次选中配置的闭包。收敛现有静态注册生成路径，避免全局 SKY_STATIC_MODULE_TARGETS 令 audit 间接初始化所有 builder/editor/runtime 模块。
- 不把任意外部预编译模块存在性误当作 CMake 已证明的部署结果；本 change 先验证仓库内模块，外部扩展需独立部署契约。

C++ 与 CMake 分别处理各自阶段的配置，但共同遵守字段、buildOption 和闭包语义，使用共享测试样例检查选择结果。无需为“统一”引入代码生成 DSL。

### 4. 审计模式保持最小激活集合

audit 配置包含产品读取所需模块，当前 CompressionModule 通过 `SKY_BUILD_COMPRESSION` 条件选择。配置定位不读取项目源配置；PreInit 继续跳过源资产挂载和 builder 初始化。审计可执行代码不能因为 cook 工厂也链接在二进制内，就激活 cook 模块或其 metadata owner。

压缩模块关闭时，未压缩产品仍按现有读取契约审计；需要但没有可用解码能力的产品明确失败。不能把“配置筛掉模块”当成产品读取一定成功。

### 5. 失败和元数据生命周期

配置错误输出文件、模块或字段及原因；启用模块缺失/初始化失败由现有 ModuleManager 返回失败，应用退出非零或使 worker 启动失败。模块成功启动但具体资产没有 builder 时，仍通过现有 cook 失败回包完成请求。

不新增 throw/try-catch、静默跳过、重复反射幂等守卫。RegisterMeta owner、definition lease 和 Init/Shutdown 会话语义保持不变。配置失败发生于模块装载前；装载失败仍走现有对称清理并收齐工作。

## Implementation Plan

先抽取解析器并迁移 Game/Editor 的重复模块解析，再接 AssetTool 两种清单，最后接通构建/部署和静态激活集合。旧配置仍保持原有字段，不进行目录重组。

本次仅记录计划。已归档 Jolt 迁移的 131 路径提交快照不加入此 change，也不加入 AssetTool 的后续实现。

## Validation

- 配置：合法/损坏/缺失文件、错误字段、重复模块、未知/关闭开关、显式禁用依赖冲突、依赖环；失败不发布半份清单。
- 宿主兼容：Game 项目优先级与项目模块、Editor 缺省行为、IPC 日志/握手/失败通知。
- 构建：只构建 AssetTool 即获得所选模块与两份配置；修改清单刷新依赖；关闭 Jolt/Animation 等功能后既不构建也不加载对应 builder。
- 原生 cook：编辑器与 worker 的启用 builder 能力一致，现有工程产品可离线生成。
- audit：只有已部署工具与产品，不提供源目录；无 builder 注册或 cook；缺失启用 DLL、解码能力或产品均失败。
- 静态：工厂可用性不等于激活，audit 不执行 cook/editor/runtime 模块 Init 或 RegisterMeta。
- 验证只报告实际运行的平台/配置，不从 Windows 动态构建推断静态或其他平台通过。
