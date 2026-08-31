## 1. 骨架与入口

- [x] 1.1 新建 `python/third_party_gui.py` 薄入口（仿 `setup.py`：argparse `-e/--engine`、创建 `QApplication`、启动主窗口）
- [x] 1.2 新建 `python/widgets/third_party_build_widget.py`，定义 `ThirdPartyBuildWidget(QMainWindow)` 空壳（窗口标题、最小尺寸、中央 widget）

## 2. 配置区

- [x] 2.1 添加 Engine Root / Intermediate / Output 三个路径输入，复用 `widgets/common/edit_with_select_wnd.py` 的 `EditWithSelect`（目录模式），并填默认值（engine 根目录、`build_3rd/intermediate`、`build_3rd`）
- [x] 2.2 添加 Platform 下拉（Win32/MacOS-x86/MacOS-arm/Linux），启动时按 `sys.platform` + `platform.machine()` 自动选中
- [x] 2.3 添加 Jobs spinbox（0=自动）、Force、Clean checkbox

## 3. 包表格

- [x] 3.1 实现读取 `cmake/thirdparty.json` 的加载函数，返回包列表
- [x] 3.2 实现平台过滤：`platforms` 缺失/为空表示全平台，否则仅保留含当前平台的包
- [x] 3.3 用 `QTableWidget` 展示包名、tag/commit、类型（header-only/static/tool），单选模式
- [x] 3.4 平台切换时刷新表格

## 4. 状态列

- [x] 4.1 复刻 `compute_package_key`（去除 `name` 后 `json.dumps(sort_keys=True)` 的 sha256 前 16 位）
- [x] 4.2 读取 `<output>/<platform>/build_metadata.json`，对比 key 与 platform，在表格末列标记「已是最新 ✓」

## 5. 构建流程

- [x] 5.1 组装 `QProcess` 命令：`[sys.executable, third_party.py, -e, -i, -o, -p, -j]` + 可选 `-f/-c/-t`
- [x] 5.2 注入 `PYTHONUTF8=1`、`PYTHONIOENCODING=utf-8` 环境变量
- [x] 5.3 实现「构建全部」（不带 `-t`）与「构建选中」（带 `-t name`）按钮
- [x] 5.4 构建开始时禁用构建按钮、显示忙碌进度指示；结束后恢复

## 6. 日志与生命周期

- [x] 6.1 添加只读等宽 `QPlainTextEdit` 日志视图
- [x] 6.2 连接 `readyReadStandardOutput`/`readyReadStandardError`，utf-8 + `errors='replace'` 解码后实时追加并自动滚动
- [x] 6.3 连接 `finished` 信号，按退出码更新状态栏成功/失败
- [x] 6.4 实现取消按钮：`QProcess.kill()` 后 Win32 `taskkill /T /F`、POSIX `pkill -P` 回收进程树

## 7. 配置持久化

- [x] 7.1 用 configparser 在关闭时保存 Engine Root/Intermediate/Output/Platform/Jobs，启动时恢复（仿 `project_manager.py`）

## 8. 验证

- [ ] 8.1 在 Win32 上运行 `python python/third_party_gui.py`，验证平台自动选中、包列表过滤、构建选中（单个 header-only 包）与实时日志
- [ ] 8.2 验证「已是最新」状态列在成功构建后刷新
- [ ] 8.3 验证取消按钮能终止进程且界面不冻结
- [x] 8.4 验证关闭重开后配置恢复
