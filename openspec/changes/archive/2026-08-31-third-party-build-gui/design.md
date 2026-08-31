## Context

SkyEngine 的三方库构建由 `python/third_party.py`（520 行 CLI）承担，负责 clone、cmake configure/build/install、复制产物、写 `build_metadata.json`、刷新 `thirdparty_cache.cmake`、归档 zip。该脚本使用 argparse 全局 `args`，无法作为库直接 import。

仓库已有一个 GUI 雏形 `python/widgets/engine_config_widget.py` 的 `EngineThirdPartyTable`，但存在：UI 线程阻塞（`subprocess.communicate()`）、无实时日志、只能整批构建、路径非标准、平台映射错误等问题。GUI 框架已确定为 PySide6（`python/requirements.txt` 已含）。

本设计新建一个独立 GUI，用 `QProcess` 流式调用现有 CLI（方案 A）。目标平台收敛到 Win32 / macOS（x86+arm）/ Linux 开发机。实施过程中发现 `third_party.py` 用 `subprocess.run(..., stdout=PIPE)` 捕获了 cmake 输出，导致中途无日志，故对其做了**最小化的输出流式改造**（见决策 9），但命令行契约与产物结构不变。

## Goals / Non-Goals

**Goals:**

- 提供一个非阻塞、实时流式日志的三方库构建 GUI。
- 保持 `python/third_party.py` 为唯一构建真相；其命令行参数、`build_metadata.json`、`thirdparty_cache.cmake`、归档产物结构不变，仅做输出流式改造。
- 支持平台选择、路径配置、Jobs/Force/Clean 选项。
- 展示包列表（按平台过滤）与增量「已是最新」状态。
- 支持「构建全部」与「构建选中」两种模式。
- 记住上次配置（configparser）。

**Non-Goals:**

- 不重构 `third_party.py` 为可 import 库（那是方案 B）。
- 不做真实进度百分比（方案 A 无法可靠获取），进度条只用忙碌态。
- 不保证取消能 100% 回收孤儿 git/cmake 子进程。
- 不覆盖 Android / iOS（移动平台构建不在本 GUI 范围）。
- 不改动 CMake 配置、`build_3rd` 输出结构或 `thirdparty.json`。

## Decisions

### 1. 进程模型：QProcess 流式调用 CLI（方案 A）

- **决策**：GUI 通过 `QProcess` 以 `sys.executable python/third_party.py ...` 方式运行，`readyReadStandardOutput`/`readyReadStandardError` 实时追加到日志视图，`finished` 信号处理退出码。
- **理由**：QProcess 天然非阻塞，避免 `subprocess.communicate()` 冻结 UI；不把 CLI 重构为库，仅依赖其命令行契约。
- **替代**：方案 B（重构 CLI 成库）——改动大、破坏 build-reference skill 的「required entrypoint」，被否决。

### 2. 参数组装

```
[sys.executable, python/third_party.py,
 -e <engine>, -i <intermediate>, -o <output>, -p <platform>, -j <jobs>]
+ [-f] 若 Force
+ [-c] 若 Clean
+ [-t <name>] 若「构建选中」
```

- **理由**：与 CLI 的 argparse 参数一一对应；`-e/-i/-o` 显式传入，避免依赖 CLI 的默认推导。

### 3. 编码处理

- **决策**：给 QProcess 注入 `PYTHONUTF8=1`、`PYTHONIOENCODING=utf-8`、`PYTHONUNBUFFERED=1` 环境变量，读回字节按 `utf-8` + `errors='replace'` 解码。
- **理由**：`third_party.py` 输出中文，Windows 控制台默认 GBK；强制 UTF-8 后按 UTF-8 解码即可，避免逐编码猜测。`PYTHONUNBUFFERED=1`（等价 `python -u`）让 `print` 立即 flush，否则 stdout 被管道接管后进入块缓冲，日志会憋到进程结束才一次性吐出。

### 4. 包表格数据源

- **决策**：直接读 `cmake/thirdparty.json`（GUI 已知道 engine 路径）填充表格，而非调用 `--list` 子进程。
- **理由**：可拿到 `platforms` 字段做平台过滤、`header_only`/`is_tool` 标类型；无额外进程开销。
- **过滤规则**：`platforms` 为空/缺省表示全平台；否则仅当当前平台在列表内才展示。

### 5. 「已是最新」状态列

- **决策**：GUI 复刻 `compute_package_key`（对 package 去除 `name` 后 `json.dumps(sort_keys=True)` 做 sha256 取前 16 位），读取 `<output>/<platform>/build_metadata.json`，若 `metadata[name].key == key` 且 `platform == 当前平台` 则标记「✓」。
- **理由**：在不改 CLI 的前提下，复用其增量缓存语义，给用户「已构建」反馈。

### 6. 平台自动选中

- **决策**：启动时按 `sys.platform` + `platform.machine()` 预选平台：win32→Win32；darwin+arm64→MacOS-arm；darwin+x86_64→MacOS-x86；linux→Linux。
- **理由**：开发机构建本机目标，减少误选。

### 7. 文件布局

- **决策**：薄入口 `python/third_party_gui.py`（仿 `setup.py`）+ 组件 `python/widgets/third_party_build_widget.py`（仿 `engine_config_widget.py`），复用 `widgets/common/edit_with_select_wnd.py` 的 `EditWithSelect`。
- **理由**：与仓库既有 Python GUI 分层风格一致。

### 8. 取消语义

- **决策**：取消时先 `QProcess.kill()`，随后 Win32 用 `taskkill /T /F /PID <pid>`，POSIX 用 `pkill -P <pid>` 尽力回收子进程树。
- **理由**：方案 A 下 python 进程会 fork git/cmake，只杀父进程会留孤儿；尽力回收是成本与收益的平衡。

### 9. CLI 输出流式改造（对 `third_party.py` 的最小改动）

- **决策**：在 `third_party.py` 中新增 `run_stream(cmd, log_file=None)` 助手，用 `subprocess.Popen` 合并 stdout/stderr 逐行流式回显到控制台并写入日志文件；将 `run_cmake` 与 `build_package_type`（header-only 分支）中的 `subprocess.run(..., stdout=PIPE, stderr=PIPE)` 全部替换为 `run_stream`。
- **理由**：原实现捕获了 cmake 输出，构建（如 boost/assimp）中途无任何日志；只有流式回显才能让 GUI 看到逐条编译输出。合并 stderr 到 stdout 使告警与错误按序内联展示。
- **替代**：仅依赖 `PYTHONUNBUFFERED=1` 只解决 `print` 缓冲，无法透出被 `PIPE` 捕获的 cmake 输出，故仍需改造 CLI。
- **影响边界**：命令行参数、`build_metadata.json`、`thirdparty_cache.cmake`、归档逻辑均不变；错误从 `CalledProcessError(stderr=...)` 变为 `CalledProcessError(returncode=...)`，日志文件仍保留 `=== configure/build/install ===` 分段。

## Risks / Trade-offs

- [孤儿 git/cmake 进程残留] → 取消时杀进程树（见决策 8）；接受极端情况下可能残留，日志/状态栏提示用户可重跑。
- [git clone / submodule 阶段静默] → 该段走 GitPython（`Repo.clone_from`），非 TTY 时 git 默认不吐进度，仍可能有一阵无输出；属 GitPython 行为，本次不处理。
- [无真实进度百分比] → 状态栏用忙碌态进度条 + 当前动作文本；完成度靠表格「✓」列与日志体现。
- [平台过滤与 metadata 路径假设] → 依赖 `cmake/thirdparty.json` 的 `platforms` 字段与 `build_3rd/<platform>/build_metadata.json` 布局；若未来 CLI 改布局需同步调整。
- [Qt 组件兼容] → 复用已有 PySide6 版本与 `EditWithSelect`，不引入新依赖。

## Migration Plan

- 纯新增工具，无迁移与回滚需求。
- 上线方式：`python python/third_party_gui.py` 或 `python -m third_party_gui`（视入口组织）。

## Open Questions

- 无阻塞性开放问题。日志视图是否附带「保存日志到文件」按钮为可选增强，默认不做。
