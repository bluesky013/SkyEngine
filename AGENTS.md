# SkyEngine Agent Rules

## 确认规则

- **commit 前必须由用户确认**：给出拟提交内容（message + 文件清单）并等待明确同意，不得自动 commit。
- **openspec archive 前必须由用户确认**：展示变更状态并等待明确同意，不得自动 archive。

## 提交组成

- **同一 change 的 spec 与代码实现一起提交**：一次提交包含实现代码 + 该 change 的 openspec 产物（proposal/design/specs/tasks、archive 目录、`openspec/specs/` 主 spec 更新）。
- **不要拆出单独的 docs / archive 提交**（不再照搬历史的 `[feat]` + `[doc]: archive ...` 两段式）。
- 提交前缀按改动性质取一个（`feat` / `fix` / `refactor` / `build` 等），不要因含 spec 就改写成 `doc`。
- 仅纯规则/文档本身的修改（如本文件）可用 `doc`。

## Review 与目录调整

- **功能修改与既有目录搬迁分开 review**：除用户明确要求同批实施，不在功能 change 中混入大范围移动、目录重命名或纯结构整理；先保留原路径，让 diff 展示实际代码修改，结构调整后续单独起 change。
- **Aurora 属于 foundation**：接口与实现均归 `engine/aurora`，不创建 `plugins/aurora`，不注册为业务 feature plugin。Renderer 实现复用现有 `engine/aurora/core`，Framework 对接归 `adaptor`，模块入口归 `runtime`；保留仅接口 target，不再建立 `renderer/{core,adaptor,runtime}` 子模块体系。新增或调整模块归属、target 和装载入口须先说明方案并取得用户确认；RHI 后端保留 `engine/aurora/rhi/{vulkan,dx12,metal,common}`。

## 异常与错误处理规范

- **禁止主动抛异常**：不得新增 `throw`（包括重新抛出 `throw;`），不得以异常作为业务或基础设施的错误返回方式。
- 可恢复错误通过明确的 `Result` / `Status` / 错误码返回；可能失败的初始化使用 `Create` / `Init` 等显式返回结果的入口，不用构造器抛异常表达失败。
- **不使用 `try/catch`**：当前不处理标准库异常，不为 `new`、容器、智能指针、线程或 future 等增加异常兜底；仍须检查业务和原生 API 的显式失败结果。
- 遇到遗留代码、回调或第三方接口实际依赖异常时，先列出调用点、影响和改为显式错误返回的方案，**由用户确认后修复**；不得自行引入或保留异常捕获作为例外。修复必须保持资源清理、任务收齐和完成通知。
- 断言仅用于程序不变量，不得用断言、终止进程或静默忽略来替代可恢复错误处理。本规则适用于新增代码及本次修改的错误路径；历史异常路径在触及时迁移，不要求无关模块一并重写。

## 类型转换规范

- **避免 `dynamic_cast`**：新增及本次修改的代码不使用 RTTI 下转型；优先虚接口、visitor、已有类型标识或明确的对象归属契约。
- 只有具体类型已由契约保证时才使用 `static_cast`。外部对象须先验证 owner/type；未知归属或类型不匹配返回显式错误，禁止机械替换或用 `reinterpret_cast` 绕过检查。
- 后端原生对象先验证 `GetDevice()` 与目标 device 一致；该 owner 契约须保证对象来自对应 device 的实现。不要增加与既有归属接口重复的 `BelongsTo`，也不要只凭 backend 枚举跨 device 转型。
- 仅迁移本次涉及的调用链；不扩大到无关历史代码。完整规则见 `.opencode/skills/skyengine-coding-rules/SKILL.md`。

## 代码注释规范

- **仅注释关键内容**：说明不易从代码直接看出的约束、原因或陷阱，例如所有权、生命周期、线程安全、关键不变量和错误处理边界；不复述命名、语法或显而易见的流程。
- **非关键设计说明单处不得超过两行**，能省则省；不得拆成相邻注释块规避长度限制。关键契约也应保持精炼，只保留正确使用和维护代码所必需的信息。
- 详细架构、设计推导、迁移计划和里程碑说明放入 `docs/` 或对应 OpenSpec change；代码中只保留必要约束，必要时附文档引用。本规则适用于新增及修改的源码、头文件、测试和构建脚本注释。

## 头文件 include 规范

- 标准库、系统/三方库及通过 target include 接口暴露的工程头使用 `<>`，保留领域路径（如 `<aurora/rdg/FrozenFrameGraph.h>`）；同 target 也遵循此规则。
- 实现私有头、测试辅助头及私有生成头使用 `""`；加入 PRIVATE include 路径或供测试访问，不改变其私有性质。源文件优先包含对应头；不得用 `../` 绕过公开模块路径或依赖边界。
- 完整规则见 [skyengine-coding-rules](.opencode/skills/skyengine-coding-rules/SKILL.md#include-delimiters-and-paths)；clang-format 仅负责排序/格式，不能代替可见性检查。

## 模块分层与跨模块交互规范

### 分层
- 以下 `engine/<feature>` 接口 / `plugins/<feature>` 实现规则用于业务 feature，不适用于 Aurora foundation；Aurora 内部仍分离接口与实现 target，且不得反向依赖业务 plugin。动态库或模块注册本身不意味着 plugin 归属。
- `engine/<feature>`：只放**接口** + **接口签名引用到的数据** + 对这批数据的纯函数；**不含实现**。
- `plugins/<feature>`：**实现**（子系统、资产 schema/序列化/注册、组件、builder、module、渲染/编辑器对接）。
- 依赖方向恒为 **plugin → engine**；`engine` 不得 include/link 任何 plugin。

### 交互归属（关键）
- 当 A 需要 B 的能力：**接口定义在 A 所在的 engine 模块（消费方）**；B 的实现（plugin）实现该接口。
  - 例：`INaviGeometryProvider` 在 `engine/navigation`；`IVegetationSurfaceProvider` 在 `engine/vegetation`；`ITerrainSystem`/`ITerrainField` 在 `engine/terrain`；`PhysicsWorld`/`PhysicsRegistry` 在 `engine/physics`。
- 接口签名/返回值用到的数据类型放**定义接口的 engine 模块**；**资产 schema/实现专用数据放 plugin**。
- 消费方通过 world 子系统名（或显式注册）解析接口指针；**出现具体实现类型即违规**。

### interface 模块
- 默认**按消费方分散**（每个 engine 模块自带其对外接口与相关数据）。
- **不做全局单一 `engine/interface`**（会导致人人依赖全部接口、耦合面最大）。仅当**同一组接口被大量模块共同依赖**时，才把**那一组**抽成独立领域接口模块。

### 命名与结构
- engine 接口 `I<Name>` / `<Name>Interface.h`；数据 `<Name>Types.h` / `<Name>Asset.h`（仅当 schema 需跨插件共享才留 engine）。
- plugin 目录 `plugins/<feature>/{runtime,editor,builder,<bridge>}`，多 target 分离（backend / render bridge / editor），参照 `plugins/bullet`、`plugins/recast`。
- 两 feature 的 bridge 放在**提供方 plugin** 的子模块。
- include 路径用领域名（`terrain/...`、`vegetation/...`），移动实现时保持不变。

### 模块类型与依赖约束（关键）

engine 内部的大模块与每个 plugin 内部，按职责拆成四类 target；一个 feature 可同时含多类，各自放独立子目录（`core/`、`editor/`、`adaptor/`、`builder/`），多 target 分离。

- **核心逻辑模块（core）**：业务逻辑与数据。**尽量只依赖 `Core`**（必要时依赖 engine 接口 / `Framework` 的接口）；**不得**依赖 editor、adaptor、builder 或具体后端/渲染。
  - 命名 `<feature>` / `<feature>.Static`；目录 `core/`。
- **编辑器模块（editor）**：编辑期工具（属性面板、inspector、cook UI 等）。**可依赖 Sandbox 的编辑器 framework**（如 `EditorCore`；规划中的 `Sandbox.framework`）；**不得被 runtime / 核心逻辑模块依赖**。
  - 命名 `<feature>.Editor`；目录 `editor/`。
- **adaptor 模块（adaptor）**：连接引擎核心与具体后端/渲染/平台。**可依赖 `Framework`、`Aurora`（及必要的 `RenderAdaptor`）**。
  - 命名 `<feature>Adaptor` / `<feature>.Adaptor`；目录 `adaptor/`。
- **builder / cook / chef 模块**：离线资产烘焙/转换（asset builder、shader compiler、cook）。**尽量只依赖自身核心模块 + 必要的三方库**；**不依赖** runtime / adaptor / editor。
  - 命名 `<feature>.Builder` / `<feature>.Cook`；目录 `builder/`、`cook/`、`chef/`。

依赖方向（自下而上）：核心逻辑 ← adaptor / editor / builder。**反向依赖一律禁止**（这三类不得被核心逻辑或 runtime 依赖）。

### 验收检查
- [ ] `engine/*` CMake 只链 `Framework`（+其它 engine 接口），**不**链任何 `*.Static`/plugin 目标。
- [ ] `engine/*` 头文件不含 plugin 头。
- [ ] 跨模块交互都有 engine 侧接口；消费方不出现具体实现类型。
- [ ] 资产 schema/实现数据不在 engine（接口引用所需除外）。
- [ ] plugin 之间不强依赖（明确单向 bridge 除外）。
- [ ] 核心逻辑模块只依赖 `Core`（+必要的 engine 接口/`Framework` 接口），不依赖 editor/adaptor/builder。
- [ ] editor 模块只经 Sandbox 编辑器 framework 接入，且不被 runtime/核心依赖。
- [ ] adaptor 模块只依赖 `Framework`/`Aurora`（+必要的 `RenderAdaptor`）。
- [ ] builder/cook/chef 模块只依赖自身核心模块 + 必要三方库。

### 反例（禁止）
- 实现写进 `engine`（如子系统/生成器/序列化）。
- 消费方直接 include 提供方 plugin 头。
- `engine` include/link plugin。
- 资产 schema/业务实现塞进 engine。
- 用单一 `engine/interface` 兜底所有接口。
- 核心逻辑模块依赖 editor / adaptor / builder 或具体后端/渲染。
- editor 模块被 runtime / 核心逻辑依赖；editor 模块绕过 Sandbox 编辑器 framework 自接 Qt 等。
- builder/cook/chef 依赖 runtime / adaptor / editor。

## 跨 DLL 单例 / 幂等（关键）

- Framework/Core（以及各 `*.Static` 库，如 `Aurora.Adaptor.Static`、`AuroraCook.Static`、`Content.Adaptor`）是**静态库**，被**每个模块 DLL 各链一份**。因此**进程级共享的单例必须继承 `sky::Singleton<T>`**（实例存在 `Environment`，跨 DLL 唯一），例：`SerializationContext`、`PhysicsBackendRegistry`、`NetworkBackendRegistry`、`ShaderCompilerFactory`、`WorldSubSystemRegistry`。
- **禁止**用函数内普通 `static T instance;` 实现跨模块共享的注册表/缓存——那会让每个 DLL 各持一份，模块注册的东西上层看不到（曾导致 world 子系统注册后编辑器列不出来）。
- **禁止**用函数内 `static bool` 做“只注册一次”的幂等守卫——**它同样是 per-DLL 的**。若守卫保护的是跨模块共享状态（如向共享 `SerializationContext` 注册类型、向共享注册表注册后端），第二个 DLL 里守卫为 `false`，会**重复注册**（`SerializationContext::Register` 的 `SKY_ASSERT(!types.count(id))` 触发，表现为编辑器加载模块时断言/挂起）。改用 `sky::Once`（`core/environment/Once.h`，实例在 `Environment`，跨 DLL 唯一）：`if (!sky::Once::Get()->Run("AuroraReflection")) { return; }`。当前 `AuroraReflection` / `ContentReflection` / `RegisterImageCookSettings` 都走 `Once`。
- 速记：**要多模块共享的状态 → `Singleton<T>`；跨模块的“只初始化一次” → `sky::Once`；纯模块内状态/纯函数内无共享的 static → 普通 static 无妨。**
- 反例：
  - `static WorldSubSystemRegistry instance;`（错）→ `class X : public Singleton<X>` + `friend class Singleton<X>;`（对）。
  - `static bool reflected = false; if (reflected) return; ...RegisterSharedTypes();`（错）→ `if (!Once::Get()->Run("XReflection")) return;`（对）。
- 排查手法：`grep -rn "static bool\|static .* instance;"` 看函数内 static；若其保护/持有的东西会跨模块被读取，即有隐患。历史已修：`AuroraReflection`/`ContentReflection`/`RegisterImageCookSettings`（Once）、`PhysicsBackendRegistry`/`NetworkBackendRegistry`/`ShaderCompilerFactory`（Singleton）。

## 编辑器配置/设置 UI（关键）

- **所有 config / 设置类 UI 必须走通用反射面板**，不得为每种 config 手搓表单/控件：
  - 数据对象用反射注册：`context->Register<T>("T").Member<&T::field>("field")...`；UI 元数据用 `.Property(...)`（`LABEL` / `ORDER` / `CATEGORY` / `RANGE_MIN/MAX/STEP` / `ASSET_TYPE` / `EDITOR_KIND` 等，见 `framework/asset/.../PropertyCommon.h` 的 `CommonPropertyKey`）。
  - 视图用 `ReflectedFormView::Bind(PropertyObject{obj, node})`（`node = GetTypeNode(*any)`）；编辑经 `CommandService`（可撤销），`SetOnEdited` 通知宿主标 dirty；`ReflectedInspectorPanel` 是 selection 驱动版。
  - 参照：世界子系统 config（`WorldConfigPanel`）、组件 inspector（`ReflectedInspectorPanel`）、`docs/editor/reflection-widget-framework.md`。
- **禁止**新增逐字段 `switch` / 自绘控件的手搓 config 表单；`PreferencesDialog` 的手搓实现是历史遗留例外，不再照搬（后续统一迁移到反射面板）。
- config 的**数据/注册在 core / plugin 侧（不含 `ui`）**，shell 侧只做 `Bind`；数据与视图分离。
- 动态、schema 驱动、无法用固定反射结构表达的设置（如 asset cook 的 per-builder settings），必须在 change 的 `design.md` 说明理由，并保持同样的数据/视图分离与可撤销编辑；**不得**把逐类型控件硬编码进面板。

## 编辑器界面（工具栏/菜单/图标）统一（关键）

- **一切 chrome 走 `EditorActionRegistry`**（跨 DLL 单例，`editor/core/extension/EditorActionRegistry.h`）：一条 `EditorAction{id,label,icon,group,menu,submenu,order,enabled,invoke}` 同时驱动**工具栏 + 菜单栏 + 快捷键**。禁止每个窗口各画一套工具栏/菜单。
- 工具栏用 `editor/shell/widgets/ToolBar`、菜单用 `widgets/MenuBar`（都消费 registry）；控件用 `UiSkin` 画（`DrawPanel/DrawToolItem/DrawField/...`），禁止手绘按钮/行。
- 图标：`engine/sandbox/resources/icons/<name>.svg`（单色白，光栅化成 `UITextureId`，按主题色 tint）；工具栏**icon-only + hover tooltip**。
- 尺寸/颜色**只来自 `UiTheme`**（`UiMetrics`/`UiColors`），禁止硬编码像素；见 `docs/editor/ui-sizing-and-scaling.md`。
- **对话框/viewer 也必须复用同一套 ToolBar/MenuBar + action**（如 asset 编辑器要有 File→Open/Close/Save 菜单 + 图标工具栏），风格与主窗口一致。
- 完整规范：`docs/editor/ui-standards.md`（含待补齐项：IconRegistry、菜单 icon/shortcut/separator、data-driven accelerator、统一 ActionBarBuilder）。

## 构建与验证（Win32 / VS 2022）

- 使用现有 `cmake-build-debug`（cache 已含 `3RD_PATH=D:/Code/sky3rd_win32_output/Win32`、`-G "Visual Studio 17 2022"`）。
- 顺序：关编辑器 → 配置 → 构建。

```bash
taskkill //IM SandboxEditor.exe //F        # 构建前必须关；否则锁 SandboxModule.dll/dxcompiler.dll 导致 post-build 拷贝失败
cmake -S . -B cmake-build-debug            # 新增源文件时必须重跑（GLOB）；已配置过可省略
cmake --build cmake-build-debug --config Debug --parallel
```

- **编辑器 UI 在 `SandboxModule.dll`（运行时加载）**：改了 shell/module 代码要编 `SandboxModule` 目标，不能只编 `SandboxEditor`。
- 只编相关目标更快，例：
  `cmake --build cmake-build-debug --config Debug --parallel --target Framework FrameworkTest EditorCoreTest EditorShellTest BulletPhysicsModule RecastNavigation SandboxModule SandboxEditor`
- 测试（gtest）：
  `output/bin/Debug/FrameworkTest.exe` · `EditorCoreTest.exe` · `EditorShellTest.exe`
- 三方库版本预检：`python python/third_party.py -p Win32 -o D:/Code/sky3rd_win32_output --check`
- 运行：`output/bin/Debug/SandboxEditor.exe`（`--project <path>.skyproj` 进编辑器；`--console` 出日志；`--frames N` 有界运行）。
- 环境备注：编辑器（IDE/工具链）在写文件时会自动跑 clang-format，可能整文件重排或挪动块；新增/改动 C++ 文件后留意 include 与括号完整性。
