# SkyEngine Agent Rules

## 确认规则

- **commit 前必须由用户确认**：给出拟提交内容（message + 文件清单）并等待明确同意，不得自动 commit。
- **openspec archive 前必须由用户确认**：展示变更状态并等待明确同意，不得自动 archive。

## 提交组成

- **同一 change 的 spec 与代码实现一起提交**：一次提交包含实现代码 + 该 change 的 openspec 产物（proposal/design/specs/tasks、archive 目录、`openspec/specs/` 主 spec 更新）。
- **不要拆出单独的 docs / archive 提交**（不再照搬历史的 `[feat]` + `[doc]: archive ...` 两段式）。
- 提交前缀按改动性质取一个（`feat` / `fix` / `refactor` / `build` 等），不要因含 spec 就改写成 `doc`。
- 仅纯规则/文档本身的修改（如本文件）可用 `doc`。

## 模块分层与跨模块交互规范

### 分层
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
