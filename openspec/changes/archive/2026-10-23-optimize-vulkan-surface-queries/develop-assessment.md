# Develop 构建模式评估（尚未实施）

建议把 Develop 定义为日常引擎/编辑器开发配置：优化代码、生成调试符号、启用引擎必要断言和诊断；保留 Debug 用于精确单步和无优化诊断，Release 用于交付。先保留 RelWithDebInfo 兼容入口，避免直接改名使已有 CMake/项目脚本失效。

## 当前证据

- `cmake-build-debug/CMakeCache.txt`：Debug 为 `/Zi /Ob0 /Od /RTC1`，Release 为 `/O2 /Ob2 /DNDEBUG`，RelWithDebInfo 为 `/Zi /O2 /Ob1 /DNDEBUG`。
- 生成的 `Core.vcxproj`：Debug 使用 `/MDd`，Release/RelWithDebInfo 使用 `/MD`；后两者不定义 `_DEBUG`。这决定跨 DLL 与预编译三方库的 CRT/STL ABI 配对。
- `cmake/options.cmake` 的 SKY_DEVELOP 被 Sandbox/Tool 开启；现有 Release 编辑器也定义 SKY_DEVELOP=1。`core/platform/BuildConfig.h` 把它作为开发设施意图，与编译器配置正交。
- `core/platform/Platform.h` 的 SKY_ASSERT/SKY_UNEXPECTED 仅依赖 `_DEBUG`，而 assert 又受 NDEBUG 影响。直接添加 Develop 名称不会启用断言。
- `cmake/thirdparty_helpers.cmake` 和 Win32 Slang 等只为 `$<CONFIG:Debug>` / `$<CONFIG:Release>` 选择库；现有 RelWithDebInfo 并不具备完整三方映射。仅设置 MAP_IMPORTED_CONFIG_DEVELOP 不能修复这些 INTERFACE link 表达式。
- `python/package_project.py` 的工具发现回退仍枚举 Debug/Release；game native 模板、presets、CPython `/LTCG` 条件也需补齐配置。

## 实施清单

1. **统一配置入口。** 在早期 CMake 配置初始化 Develop 的 C/C++、链接器和 MSVC runtime 选项；多配置生成器加入 CMAKE_CONFIGURATION_TYPES，单配置支持 CMAKE_BUILD_TYPE=Develop。初始可沿用 RelWithDebInfo 的优化/PDB 设置，明确不启用 `/RTC1`、不强行定义 `_DEBUG`。
2. **修复三方映射。** 统一 Debug→Debug，Develop/RelWithDebInfo/Release/MinSizeRel→Release。审计 helpers、assimp、cpython、tracy、Slang 及动态库部署；真正 IMPORTED 库用 MAP_IMPORTED_CONFIG，INTERFACE 的 CONFIG 表达式需独立改写。CPython Release 静态对象所需 `/LTCG` 扩展到优化配置。
3. **分离断言与 CRT。** 引入独立引擎断言策略，Develop 开启必要不变量检查，慢检查另控，避免通过 `_DEBUG` 切换到 Debug CRT。处理 NDEBUG 与标准 assert 的关系；审计 SKY_ASSERT 表达式的副作用，不能改变业务执行语义或以断言替代可恢复错误。
4. **保留诊断并控制成本。** SKY_DEVELOP 继续描述编辑器/工具设施，明确其与构建配置的组合。Tracy、GPU validation、资源命名各自可控；性能基线明确记录这些开关。当前 EditorRenderer 强制 enableValidation=true 需在独立配置工作中接入可控入口。
5. **打通产物与项目工具。** 输出 `output/bin/Develop`、保留匹配 DLL/EXE 的 PDB；补根 presets、game 模板、install/export、三方 DLL 部署、chef/launcher/package 工具选择与帮助文档。避免包装脚本误选旧 Debug 工具。
6. **验证矩阵。** Windows Debug/Develop/Release 编译和关键测试；SandboxModule 动态装载、shader cook、示例项目打包；检查 CRT/STL ABI 与符号加载。其它平台的单/多配置生成器至少配置/编译验证，实机运行单独记录。性能比较固定场景及 validation/profiling 开关。

## UE 与 CMake 参考

UE Development 启用大多数优化，并用于常规编辑器开发；DebugGame 则将引擎与游戏模块的优化策略分开。适合借鉴其“开发配置保留诊断、目标类型与配置分离”的原则，本项目暂不需要复制全部 UE 配置矩阵。[UE Build Configurations](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-configurations-reference-for-unreal-engine)

UE 将 check、slow check、verify 区分；SkyEngine 应明确自己的断言与求值契约后实现，不能只改宏名称。[UE Asserts](https://dev.epicgames.com/documentation/en-us/unreal-engine/asserts-in-unreal-engine)

自定义配置需初始化对应大写配置变量；导入配置映射只适用于相关 imported target 属性。[CMake Buildsystem](https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html)、[MAP_IMPORTED_CONFIG](https://cmake.org/cmake/help/latest/prop_tgt/MAP_IMPORTED_CONFIG_CONFIG.html)
