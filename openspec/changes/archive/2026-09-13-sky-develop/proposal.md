## Why

当前「开发构建」没有独立标志：`_DEBUG` 混绑了「完整 Debug 配置（重断言、慢）」和「开发/编辑器构建（要资源名、校验等开发设施，但希望接近发布性能）」两种诉求。`SKY_ENABLE_RESOURCE_NAME` 目前临时用 `_DEBUG` 判定（见 `aurora-resource-name`），但这意味着 release 与 editor 工具链无法享受资源名、也把「要不要开发设施」绑定到了编译配置而非构建意图。

需要一个独立的 `SKY_DEVELOP` 标志，统一表达「develop 模式」，让资源名、额外校验、调试 UI 等开发者设施挂在它上面，与 `_DEBUG`（编译器 debug 配置）解耦。

## What Changes

- 引入 **`SKY_DEVELOP` 宏**（0/1，默认 `0`）：表示「develop/开发构建」意图。
- **构建系统接入**：CMake 提供选项（如 `SKY_DEVELOP` 开关，editor/tool 构建默认开），开启时 `add_compile_definitions(SKY_DEVELOP=1)`；未定义时 config 头 `#ifndef` 兜底 `0`。
- **迁移既有开关**：`SKY_ENABLE_RESOURCE_NAME` 改判 `SKY_DEVELOP`（替换临时的 `_DEBUG`），后续开发设施统一挂在 `SKY_DEVELOP` 上。
- 文档明确 `_DEBUG`（编译器 debug 配置）与 `SKY_DEVELOP`（构建意图）的分工。

## Capabilities

### New Capabilities

- `sky-develop`: 构建意图标志——`SKY_DEVELOP` 宏（默认 0）、CMake 接入、与 `_DEBUG` 解耦，作为开发者设施（资源名/校验/调试 UI）的统一开关。

### Modified Capabilities

（无既有 spec 修改）

## Impact

- **新增/修改**：core 侧 config 头定义 `SKY_DEVELOP`；`cmake/configuration.cmake` 加选项与 `add_compile_definitions`；`aurora-resource-name` 改判 `SKY_DEVELOP`。
- **依赖**：无；被 `aurora-resource-name`（及后续开发设施）依赖。
- **边界**：属构建/核心配置层，不涉及 RHI 接口。
