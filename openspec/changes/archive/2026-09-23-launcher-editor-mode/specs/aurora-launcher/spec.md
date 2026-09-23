## ADDED Requirements

### Requirement: Launcher 编辑器模式

The launcher SHALL support an editor application mode selected by a start argument (`--app editor`) that runs the
editor host, in addition to its game and XR modes.

#### Scenario: Start the editor
- **WHEN** the launcher is started with `--app editor`
- **THEN** it SHALL run the editor host and open the editor window

#### Scenario: Default remains the game mode
- **WHEN** the launcher is started without `--app editor`
- **THEN** it SHALL run the game (or XR) application as before

### Requirement: Launcher 游戏模式从部署的 configs 启动

Outside editor/tool/Android builds the game application SHALL use the executable bundle path as its work file
system, so the builtin `configs/` deployed next to the launcher are found and the launcher boots without a
project directory.

#### Scenario: Boot from deployed configs
- **WHEN** the launcher runs in game mode with `configs/modules_game.json` next to the executable
- **THEN** it SHALL load the config from the bundle path and register its modules without crashing
