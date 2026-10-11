## 1. Module static/dynamic split convention

- [ ] 1.1 确立约定：每个模块提供静态核心 `<Name>` 与动态注册 shim `<Name>.Dynamic`（检查现有模块是否符合，列出偏差）
- [ ] 1.2 定义打包模式枚举与规则数据结构（`dynamic` | `static` | `mixed` + 静态模块清单/分组）
- [ ] 1.3 引擎侧 CMake 选项：`SKY_MODULE_PACKING`、`SKY_STATIC_MODULES`（及对应 `cmake/*.cmake` 处理）

## 2. Engine build support

- [ ] 2.1 提供统一 helper，按模式为每个模块决定：产出 SHARED shim / 链接静态核 / 编译期内联注册
- [ ] 2.2 `plugin.json`/模块声明支持"可静态化"标记，供规则选择（不逐个改模块逻辑）
- [ ] 2.3 处理重复符号与注册顺序（静态对象库 + 可见性；确定性注册顺序）
- [ ] 2.4 静态模式遵守编辑器分层：`requires_editor` 的模块不进入 runtime 静态包（`engine-plugin-layering`）

## 3. Runtime resolution

- [ ] 3.1 `ModuleManager`：静态提供的模块优先解析；同名动态库存在时不重复注册
- [ ] 3.2 错误处理：静态集合与实际注册不一致（缺失/重复/循环依赖）时给出明确诊断

## 4. Packaging integration

- [ ] 4.1 `export_presets.json` 扩展：`packing` 模式 + `staticModules` 规则，映射到 `SKY_MODULE_PACKING`/`SKY_STATIC_MODULES`
- [ ] 4.2 `python/package_project.py` 透传打包模式/规则（桌面 → cmake `-D`；Android → gradle arguments）
- [ ] 4.3 iOS/受限平台：预设可默认 `static`（平台约束）
- [ ] 4.4 项目 `native/CMakeLists.txt` 接收并落实模式与规则

## 5. Verification & docs

- [ ] 5.1 单测：模式选择、混合规则、静态模块仍注册、无重复注册
- [ ] 5.2 端到端：同一工程在 `dynamic` 与 `static` 下各出一次 pack（对比 .so 数量/加载）
- [ ] 5.3 文档：模块打包指南（模式、规则、平台约束），更新 `docs/features/game-project-build.md`
