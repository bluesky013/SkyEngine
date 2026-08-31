## Why

SkyEngine 的三方库构建目前只有命令行工具 `python/third_party.py`，需要开发者记忆 `-i/-o/-e/-p/-j/-f/-t` 等参数组合。已有的 `python/widgets/engine_config_widget.py` 虽有一个雏形界面，但会阻塞 UI 线程、无实时日志、只能整批构建、路径与标准流程不一致。需要一个独立的、非阻塞、流式输出的三方库构建 GUI，覆盖 Win32 / macOS / Linux 开发机。

## What Changes

- 新增一个独立的三方库构建 GUI，通过 `QProcess` 流式调用 `python/third_party.py`（不改动 CLI 本身）。
- 新增薄入口 `python/third_party_gui.py`（仿 `python/setup.py`）。
- 新增组件 `python/widgets/third_party_build_widget.py`（仿 `python/widgets/engine_config_widget.py`）。
- 支持平台选择 `Win32 / MacOS-x86 / MacOS-arm / Linux`，启动时按当前系统自动选中。
- 支持配置 Engine Root / Intermediate / Output 路径、Jobs（0=自动）、Force、Clean。
- 包表格读取 `cmake/thirdparty.json`，按当前平台过滤，单选，并显示每个包「已是最新」状态（读 `build_metadata.json`）。
- 支持「构建全部」与「构建选中」两种模式。
- 实时流式日志输出到只读文本视图，非阻塞。
- 记住上次使用的路径/平台/Jobs 配置（configparser）。

## Capabilities

### New Capabilities

- `third-party-build-gui`: 通过图形界面流式调用现有 `third_party.py` CLI 构建三方库的能力，包括平台选择、包列表展示与状态、增量状态提示、实时日志、构建控制。

### Modified Capabilities

<!-- 无现有 spec 需求变更 -->

## Impact

- 新增 `python/third_party_gui.py`、`python/widgets/third_party_build_widget.py`。
- 复用 `python/widgets/common/edit_with_select_wnd.py` 的 `EditWithSelect` 组件。
- 依赖已存在的 `PySide6`（`python/requirements.txt` 中已含）与 `python/third_party.py` CLI。
- 不修改 `python/third_party.py` 及其调用契约，保持其为唯一构建真相。
- 不修改 CMake 配置或 `build_3rd` 输出结构。
