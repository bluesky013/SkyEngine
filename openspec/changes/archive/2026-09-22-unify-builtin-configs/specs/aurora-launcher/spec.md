## MODIFIED Requirements

### Requirement: 引擎内置 configs

引擎 SHALL 提供内置 `configs/`（仓库根，`modules_game.json` / `modules_editor.json` / `asset_build_presets.json` / `render_preload_assets.json`），并在构建时部署到可执行文件旁的 `configs/`。`GameApplication` SHALL 在项目 workFs 找不到 `configs/modules_game.json` 时回退到内置配置，使 launcher 无需外部工程即可启动。

#### Scenario: 无工程启动

- **WHEN** 直接运行 launcher 且无 `-p` 工程（或工程无 configs）
- **THEN** 从可执行文件旁的内置 `configs/modules_game.json` 加载模块配置
