## Why

Vulkan 查询热点修复后，Debug 的 GUI/快照与图编译仍明显慢于优化配置。需要可日常开发、可调试且保留引擎断言的 Develop 配置；现有 RelWithDebInfo 的三方库映射不完整，不能只重命名。

## What Changes

- 新增 Develop 编译配置：优化、符号、Release CRT/三方 ABI，保留 Debug/Release/RelWithDebInfo。
- 引擎断言与 `_DEBUG`/NDEBUG 解耦，保持断言表达式的单次求值契约。
- GPU validation 与资源命名提供独立 AUTO/ON/OFF 构建策略；Tracy 沿用现有开关。
- 修复三方映射、补根/项目模板 presets、Develop 产物路径和 cook 工具选择。
- 验证配置、真实 Develop 编辑器/模块/cook 构建、断言和工具选择回归，并记录平台边界。

## Capabilities

### New Capabilities

- `engine-build-configurations`: Develop 配置、优化与符号、CRT/三方映射、断言/诊断策略和工具链闭环。

### Modified Capabilities

- `sky-develop`: 区分开发设施默认开关与可单独配置的诊断策略，保留其与 `_DEBUG` 的正交关系。

## Impact

现有 CMake 配置/helper/Find 模块、Core 断言头、Renderer 初始化、game 模板、Python cook 工具发现及相应测试。无模块/target 归属变更、目录迁移或新增业务接口。承接已提交 `optimize-vulkan-surface-queries` 的 Develop 评估；不改变 Pick 或其它 renderer 功能 change。
