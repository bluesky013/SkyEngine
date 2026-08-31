## ADDED Requirements

### Requirement: 平台选择

系统 SHALL 提供平台下拉选择，包含 `Win32`、`MacOS-x86`、`MacOS-arm`、`Linux` 四项。系统 SHALL 在启动时根据当前系统自动预选对应平台。

#### Scenario: 自动预选平台

- **WHEN** 界面启动且当前系统为 Windows
- **THEN** 平台下拉默认选中 `Win32`

#### Scenario: 自动预选 macOS 架构

- **WHEN** 界面启动且当前系统为 macOS
- **THEN** 根据 `platform.machine()` 选中 `MacOS-arm`（arm64）或 `MacOS-x86`（x86_64）

### Requirement: 路径配置

系统 SHALL 允许用户配置 Engine Root、Intermediate、Output 三个路径，均支持目录浏览选择。系统 SHALL 提供合理默认值。

#### Scenario: 选择目录

- **WHEN** 用户点击路径旁的浏览按钮并选择目录
- **THEN** 对应路径输入框更新为所选目录

### Requirement: 构建选项

系统 SHALL 提供 Jobs（整数，0 表示自动）、Force（布尔）、Clean（布尔）三个构建选项，并在构建时映射到 CLI 的 `-j`、`-f`、`-c` 参数。

#### Scenario: 选项映射

- **WHEN** 用户勾选 Force 并以 Jobs=8 发起构建
- **THEN** 系统以 `-j 8 -f` 参数调用 `third_party.py`

### Requirement: 包列表展示

系统 SHALL 读取 `cmake/thirdparty.json` 并展示包列表，按当前选中平台过滤（`platforms` 为空或缺失表示全平台）。系统 SHALL 展示包名、tag/commit、类型（header-only/static/tool）。

#### Scenario: 平台过滤

- **WHEN** 用户选择 `Win32` 平台
- **THEN** 表格仅展示 `platforms` 含 `Win32` 或未指定 `platforms` 的包

### Requirement: 已是最新状态

系统 SHALL 读取 `<output>/<platform>/build_metadata.json`，通过复刻 `compute_package_key` 的哈希逻辑，判断每个包是否已构建为最新，并在表格中展示「已是最新」状态。

#### Scenario: 已构建包标记

- **WHEN** 某包的 metadata `key` 与当前配置计算所得 key 一致且平台匹配
- **THEN** 该包在表格中显示「已是最新」标记

### Requirement: 构建全部与构建选中

系统 SHALL 提供「构建全部」与「构建选中」两种构建模式。构建全部不传 `-t`，构建选中传入 `-t <选中包名>`。

#### Scenario: 构建选中

- **WHEN** 用户在包表格中选中一个包并点击「构建选中」
- **THEN** 系统以 `-t <包名>` 参数调用 `third_party.py`

#### Scenario: 构建全部

- **WHEN** 用户点击「构建全部」
- **THEN** 系统不传 `-t` 参数调用 `third_party.py`

### Requirement: 实时日志

系统 SHALL 通过 QProcess 异步运行 CLI，并将 stdout/stderr 实时追加到只读日志视图，构建过程中 UI 不得阻塞。

#### Scenario: 流式输出

- **WHEN** CLI 在构建过程中输出日志
- **THEN** 日志视图实时追加该输出且界面保持可响应

### Requirement: 构建生命周期

系统 SHALL 在构建开始时禁用构建按钮、显示忙碌进度指示，在 `finished` 信号后根据退出码显示成功或失败，并支持取消（尽力回收进程树）。

#### Scenario: 构建成功

- **WHEN** CLI 进程以退出码 0 结束
- **THEN** 状态栏显示构建成功

#### Scenario: 构建失败

- **WHEN** CLI 进程以非 0 退出码结束
- **THEN** 状态栏显示构建失败

#### Scenario: 取消构建

- **WHEN** 用户点击取消按钮
- **THEN** 系统终止 CLI 进程并尽力回收其子进程树

### Requirement: 配置持久化

系统 SHALL 在关闭时保存 Engine Root、Intermediate、Output、平台、Jobs 配置，并在下次启动时恢复。

#### Scenario: 恢复配置

- **WHEN** 用户上次设置过路径与平台后关闭再重新启动
- **THEN** 界面恢复上次保存的路径与平台配置
