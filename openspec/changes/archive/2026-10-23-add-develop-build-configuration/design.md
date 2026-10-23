## Context

基线 `0bf7b53f` 已消除稳定帧格式枚举。Develop 需要提供接近优化配置的 CPU 性能与可调试性。现有 SKY_DEVELOP 是编辑器/工具设施意图；根与 game 模板共用 cmake/configuration.cmake，但三方 INTERFACE targets 多处仅匹配 Debug/Release。

当前代码的 EditorRenderer/AuroraModule 已用 `_DEBUG` 控制 GPU validation，RendererInitInfo 默认 false；上一轮评估中“强制 true”的描述来自更早采样版本，本次以当前源码为准。

## Goals / Non-Goals

**Goals:** 真实 Develop 配置、符号、正确 CRT/三方映射、独立断言与诊断策略、preset/工具发现闭环；保留已有配置。

**Non-Goals:** 不复制 UE 全部目标/配置矩阵，不新增 DebugGame，不迁移模块，不引入全局异常策略、通用配置 UI 或新的业务 target；不扩展 package_project 当前不支持的 Windows 发布格式。

## Decisions

1. 在现有 configuration 入口引入 build_configurations.cmake，语言启用后初始化 Develop C/C++/ObjC++ 优化符号与链接器参数，保留用户覆盖。多配置追加 Develop；单配置显式 CMAKE_BUILD_TYPE=Develop。MSVC 使用 Release CRT，默认 `/O2 /Ob2 /Zi /DNDEBUG`，不定义 `_DEBUG`、不启用 `/RTC1`；其它编译器继承 RelWithDebInfo。链接符号沿用 RelWithDebInfo。输出为各平台既有输出目录下 Develop 子目录。
2. 三方非 Debug 配置选择 Release archives；真正 imported targets 配置映射优先同名 Develop，再 RelWithDebInfo/Release，Slang 的预编译包直接映射 Release。修复 INTERFACE genex，列表逐项处理，避免只设置 MAP_IMPORTED_CONFIG 仍丢库；CPython Release 静态对象继续要求 LTCG。
3. `SKY_BUILD_DEVELOP` 只表示配置。SKY_DEVELOP 仍由编辑器/工具或显式选项开启；game Develop preset 显式开启该设施意图，不把 `_DEBUG` 当代用品。引擎 assert 使用独立 `SKY_ENABLE_ASSERT`，AUTO 在 Debug/Develop 开启，Release/RelWithDebInfo 关闭；开启时不依赖标准 assert/NDEBUG，失败输出表达式/位置并终止，仅限程序不变量。关闭时仍按历史契约求值一次，不改变副作用；不以断言处理可恢复业务错误。
4. CMake `SKY_ASSERTIONS`、`SKY_GPU_VALIDATION`、`SKY_RESOURCE_NAMES` 为 AUTO/ON/OFF。validation AUTO 仅 Debug 开启，便于 Develop 日常性能；names AUTO 跟随 SKY_DEVELOP；Tracy 沿用 SKY_USE_TRACY。所有宿主初始化消费同一宏，不新增 settings UI。
5. 根 presets 提供日常 Develop 与 Debug/Release 构建入口。game 模板补 Develop 产物路径及各平台 Develop preset，保留原有发布 presets。Python cook 工具优先已构建的 Develop，再兼容旧 Debug/Release/RelWithDebInfo 与平铺布局，显式 tool 参数仍优先。不修改遗留异常依赖的发布错误路径。
6. 验证包括独立 CMake consumer 的单/多配置检查、Core 断言求值/终止测试、工具优先级测试、真实 Develop 编辑器及 cook targets、Debug/Release 回归、RelWithDebInfo 链接兼容。复用已有 CoreTest/Python tests，不新增产品 target。Windows 实机运行；无对应 SDK 的平台只声明配置级验证。

实施审查补充：Vulkan 的 debug-utils 扩展由 validation 或资源命名共同请求，messenger/layer 仍只随 validation 开启；缺失扩展时不把 messenger create-info 放入 pNext。真实模板构建发现 Windows GUI launcher 的 `main` 缺少入口链接选项，沿用 SandboxEditor 的 `mainCRTStartup`，不改变 launcher target/模块归属。

VS solution 补充验证：Core/Framework 测试原先创建了与同级库项目同名的虚拟 folder，VS 加载后会丢弃冲突项目，导致 Develop 静态库未生成。测试的 FOLDER 改为 `Engine/Foundation/Tests/{Core,Framework}`；仅调整 IDE 虚拟分组，不移动源码、不重命名 target。验收同时覆盖 VS solution 加载/整解构建，不能仅用单 target 的 MSBuild 成功替代。

## Risks / Trade-offs

- 混装不同配置的 DLL → Develop 独立目录，SandboxEditor 既有 manifest 依赖拉齐所有模块，启动验证记录加载与退出。
- 宏影响头文件 ABI → 不改变 SKY_DEVELOP 默认设施规则；配置与诊断定义通过共享配置入口一致应用，外部手工编译需遵循同一设置。
- 优化导致局部变量不可见/单步跳行 → 保留完整 Debug；Develop 不承诺无优化单步体验。
- 标准 assert 受 NDEBUG 关闭 → 自有断言只用于不变量，测试实际失败行为和 release 求值；不重新定义 NDEBUG 或伪造 `_DEBUG`。
- 新配置首次完整构建成本较高 → 使用现有 build tree 和预编译 Release 三方库，不重编第三方。

## Migration Plan

无数据迁移。新增配置而保留 RelWithDebInfo 兼容入口；已有 Debug/Release 命令不变。实施与验证通过后由用户确认提交，不自动 archive。

## References

- [CMake custom configurations](https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#build-configurations)
- [CMake MSVC runtime library](https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html)
- [CMake imported configuration mapping](https://cmake.org/cmake/help/latest/prop_tgt/MAP_IMPORTED_CONFIG_CONFIG.html)
