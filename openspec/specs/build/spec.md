# build Specification

## Purpose

Define build configurations, dependency pinning, source ownership and retirement of legacy build stacks.

## Scope notes

### editor-build

Keep editor runtime modules coordinated with host-only builds in the selected configuration, preventing stale dynamically loaded binaries.

### engine-build-configurations

Define optimized Develop builds, compatible dependency selection, independent diagnostic policies, and development tool discovery.

### third-party-version-pinning

三方构建机制的版本钉住：保证 intermediate 中的 checkout 始终与 `thirdparty.json` 配置的 tag/commit 一致，引擎代码按钉住版本的 API 编写。

## Requirements

### Requirement: Core memory resources use the standard library

`engine/core` SHALL use `std::pmr` for its memory-resource aliases and SHALL NOT depend on
`boost::container`.

#### Scenario: Core builds without Boost

- **WHEN** `engine/core` is built
- **THEN** it SHALL NOT include Boost headers or link a Boost library

#### Scenario: PMR aliases resolve to the standard library

- **WHEN** code uses `PmrResource`, `PmrVector`, `PmrList`, `PmrString`, or `PmrHashMap`
- **THEN** they SHALL resolve to the corresponding `std::pmr` types

### Requirement: Framework does not depend on Boost

`engine/framework` SHALL NOT include or link Boost.

#### Scenario: Framework builds without Boost

- **WHEN** `engine/framework` is built
- **THEN** it SHALL NOT include Boost headers or link a Boost library

### Requirement: Boost is removed from the build

Boost SHALL NOT be discovered, linked, or required by the project; the legacy shader/render modules that used it
have been removed.

#### Scenario: No Boost target is discovered

- **WHEN** the project is configured
- **THEN** no Boost target SHALL be created or linked

#### Scenario: No compiled Boost library required

- **WHEN** the third-party bootstrap runs
- **THEN** no Boost package SHALL be required to build the engine

### Requirement: The legacy compiler link on Aurora is removed

`engine/aurora/core` SHALL NOT link the legacy `ShaderCompiler` target.

#### Scenario: Aurora builds without the legacy compiler

- **WHEN** the Aurora core target is built
- **THEN** it SHALL NOT pull in `ShaderCompiler.Static`

### Requirement: Editor host build schedules available configured runtime modules

When SandboxEditor is configured and configs/modules_editor.json is available, the top-level build SHALL add a build-only dependency from SandboxEditor to each module name in that manifest that has an existing configured CMake target. Dependency binding SHALL occur after engine/plugin targets are defined. Building SandboxEditor SHALL schedule those dependencies through the same selected build configuration without adding module link dependencies or changing runtime loading behavior.

#### Scenario: Host-only Debug build includes configured plugins
- **WHEN** an enabled manifest module such as BulletPhysicsModule or RecastNavigation has a configured target and SandboxEditor is built in Debug
- **THEN** the generated build graph SHALL schedule that target in the selected configuration before completing the host build

#### Scenario: Optional target is unavailable
- **WHEN** the manifest names a module without a configured target
- **THEN** dependency binding SHALL skip that name without force-enabling a plugin or claiming the module is available at runtime

#### Scenario: Editor host is not configured
- **WHEN** the current configuration has no SandboxEditor target
- **THEN** runtime-module dependency binding SHALL NOT require that target or prevent the configuration from succeeding

### Requirement: Manifest edits refresh the editor dependency selection

The checked-in module manifest SHALL remain the source of the editor's runtime module dependency selection when SandboxEditor is configured. CMake SHALL track it as a configure input so a later change refreshes the generated dependency set on the next build.

#### Scenario: Configured module set changes
- **WHEN** modules_editor.json changes after configuration
- **THEN** the next build SHALL trigger configuration as needed and use the updated set of existing module targets

### Requirement: Startup acceptance identifies the actual host and module configuration

Verification of this ABI-mismatch repair SHALL record a host and configured runtime modules built from the same Debug tree/configuration, together with actual startup exit/crash/log evidence. A completed build SHALL NOT itself be reported as runtime or GPU/display acceptance. The build guarantee SHALL NOT extend to arbitrary external modules or binaries replaced after the build.

#### Scenario: Former stale Bullet module is rebuilt
- **WHEN** the Debug startup is rerun after the host and affected configured modules are rebuilt
- **THEN** the verification record SHALL identify the relevant binary/path evidence and whether the former PostTick dispatch into RTTI recurs

#### Scenario: Startup reaches a separate platform limitation
- **WHEN** module loading is consistent but GPU/display creation cannot complete
- **THEN** verification SHALL distinguish that limitation from the repaired module ABI fault and retain the corresponding platform gate

### Requirement: Optimized Develop configuration with symbols

The engine and generated game CMake entry SHALL support Develop in single- and multi-configuration generators, with optimization and debug symbols. Develop SHALL use the Release CRT/STL ABI and prebuilt Release third-party libraries without defining `_DEBUG`. Debug, Release, RelWithDebInfo and MinSizeRel SHALL remain available. Runtime binaries SHALL use the Develop output subdirectory on desktop.

#### Scenario: Build a Develop editor
- **WHEN** the user builds SandboxEditor with configuration Develop
- **THEN** the host and configured runtime modules are built consistently, link Release third-party libraries and produce optimized binaries with debug symbols in output/bin/Develop

#### Scenario: RelWithDebInfo compatibility
- **WHEN** an existing consumer selects RelWithDebInfo
- **THEN** it retains that configuration and receives compatible Release third-party libraries rather than missing links or Debug CRT libraries

#### Scenario: Open a generated Visual Studio solution with tests
- **WHEN** the engine solution is loaded in Visual Studio with test targets enabled
- **THEN** projects and solution folders have distinct names within each parent, Core and Framework remain loaded with Develop build mappings, and dependent targets build their required libraries

### Requirement: Assertion policy is independent of CRT and NDEBUG

Engine assertions SHALL be enabled by default in Debug and Develop and disabled by default in Release/RelWithDebInfo. Enabled engine assertions SHALL detect failed invariants even when NDEBUG is defined. Assertion expressions SHALL be evaluated once in both enabled and disabled configurations, preserving existing side effects. Assertion failures SHALL identify the expression and source location; recoverable errors SHALL remain explicit results.

#### Scenario: Develop invariant fails
- **WHEN** a Develop engine assertion evaluates false with NDEBUG defined
- **THEN** it reports the invariant and terminates without silently passing

#### Scenario: Disabled assertion expression
- **WHEN** an assertion with a side effect executes in Release
- **THEN** the side effect occurs exactly once and no assertion termination occurs

### Requirement: Diagnostic configuration and development tool discovery

CMake SHALL provide AUTO/ON/OFF policies for engine assertions, GPU validation and resource names. AUTO SHALL enable GPU validation only in Debug and derive resource names from SKY_DEVELOP. Invalid policy values SHALL fail configuration. Existing profiling and explicit tool overrides SHALL remain supported. Root/game presets SHALL expose Develop without replacing publishing presets. Cook tool discovery SHALL prefer existing Develop tools and retain legacy configuration fallbacks.

#### Scenario: Develop GPU validation override
- **WHEN** Develop is configured with SKY_GPU_VALIDATION=ON
- **THEN** the editor and runtime host request GPU validation consistently without changing CRT configuration

#### Scenario: Discover tools with multiple builds present
- **WHEN** both Develop and Debug cook tools exist and no explicit tool is supplied
- **THEN** discovery selects the Develop tool with its sibling modules; if absent it uses a supported fallback

#### Scenario: Vulkan resource naming without validation
- **WHEN** resource names are enabled and GPU validation is disabled
- **THEN** Vulkan requests debug utilities when supported, without enabling validation layers or a debug messenger

### Requirement: Engine module tests are colocated with their module

Test sources, test helpers, and test fixtures for an engine module SHALL live under that module's directory (for example `engine/core/test/` and `engine/framework/test/`) and SHALL be configured by that module's CMake when `SKY_BUILD_TEST` is enabled. Engine tests SHALL NOT depend on a repository-wide `engine/test` aggregator or reach into another module's test directory by source path.

#### Scenario: Core tests are configured by Core
- **WHEN** `SKY_BUILD_TEST` is enabled
- **THEN** `engine/core` SHALL configure `CoreTest` from `engine/core/test`, and no central test directory SHALL be required

#### Scenario: Framework tests are configured by Framework
- **WHEN** `SKY_BUILD_TEST` is enabled
- **THEN** `engine/framework` SHALL configure `FrameworkTest` and its helpers from `engine/framework/test`

#### Scenario: UI tests are configured by UI
- **WHEN** `SKY_BUILD_TEST` is enabled
- **THEN** `engine/ui` SHALL configure its UI test targets from `engine/ui/test`, including the embedded UI shader contract test already stored with the UI render sources

#### Scenario: No cross-module test source paths
- **WHEN** module test CMake is inspected
- **THEN** it SHALL NOT reference sources under another engine module's `test` directory through `${ENGINE_ROOT}/engine/...`

### Requirement: Framework test helpers move with FrameworkTest

The `ProcessHelper` and `CookWorkerStub` executables, the framework fixture assets/configs, and the generated engine/project root header SHALL live in `engine/framework/test` and SHALL be built only with `FrameworkTest`. The generated project root SHALL point at the framework test fixture root.

#### Scenario: Helper executables deploy beside FrameworkTest
- **WHEN** `FrameworkTest` is built
- **THEN** `ProcessHelper` and `CookWorkerStub` SHALL be built and deployed next to it, preserving the process and out-of-process cook tests

#### Scenario: Fixture root survives relocation
- **WHEN** `AssetManagerTest` resolves its project fixture files
- **THEN** the generated `PROJECT_ROOT` SHALL point at the relocated framework test fixtures

### Requirement: Existing test target names remain stable

The colocation move SHALL preserve the existing test executable names: `CoreTest`, `FrameworkTest`, `UICoreTest`, `UITextTest`, `UITextFreeTypeTest`, `UIEditorTest`, `UIShaderTest`, `ProcessHelper`, and `CookWorkerStub`.

#### Scenario: Documented test commands still resolve
- **WHEN** a developer runs a previously documented test target name
- **THEN** the same test executable SHALL exist after the move

### Requirement: Legacy render tests are removed

The colocation change SHALL remove `RenderTest` and `VulkanTest`, which only exercised the deprecated legacy `RenderCore`/`VulkanRHI` stack. The legacy render/shader sources and their opt-in switch SHALL also be removed by `legacy-stack-retirement`.

#### Scenario: Default build has no legacy test targets
- **WHEN** the project is configured with the default options
- **THEN** no `RenderTest`, `VulkanTest`, or `SKY_BUILD_LEGACY_RENDER` branch SHALL exist

#### Scenario: No legacy source tree remains
- **WHEN** the repository is inspected after this change and `legacy-stack-retirement` lands
- **THEN** no `engine/render`, `engine/shader`, `RenderTest`, or `VulkanTest` source SHALL remain

### Requirement: Legacy source trees are removed

The repository SHALL NOT contain the pre-Aurora renderer (`engine/render`), the pre-Aurora shader compiler (`engine/shader`), or the legacy Qt editor (`engine/editor`). Aurora SHALL remain the renderer and shader implementation, and the non-Qt sandbox SHALL remain the editor implementation.

#### Scenario: Legacy engine trees are absent
- **WHEN** the repository tree is inspected
- **THEN** `engine/render`, `engine/shader`, and `engine/editor` SHALL NOT exist

#### Scenario: Aurora and sandbox remain
- **WHEN** the default desktop build is configured
- **THEN** `engine/aurora` and `engine/sandbox` SHALL still be configured and their targets SHALL remain available

### Requirement: Legacy build options are removed

The build SHALL NOT expose `SKY_BUILD_LEGACY_RENDER` or `SKY_BUILD_EDITOR`. The sandbox editor SHALL be selected only through `SKY_BUILD_SANDBOX`, and no CMake path SHALL call `find_package(Qt5)`.

#### Scenario: Legacy options are rejected
- **WHEN** a user configures with `-DSKY_BUILD_LEGACY_RENDER=ON` or `-DSKY_BUILD_EDITOR=ON`
- **THEN** the option SHALL have no legacy target or build branch behind it

#### Scenario: Qt5 is not probed
- **WHEN** the project is configured without `SKY_BUILD_EDITOR`
- **THEN** CMake SHALL NOT search for Qt5 or define Qt editor targets

#### Scenario: Sandbox editor selection is unchanged
- **WHEN** `SKY_BUILD_SANDBOX=ON` is configured
- **THEN** the sandbox editor targets SHALL be configured as before

### Requirement: Legacy-only consumers are removed

The XR plugin, the PVS legacy-render glue, and the legacy-only developer tools SHALL be removed. The PVS core visibility logic SHALL remain available without any render dependency.

#### Scenario: XR plugin is gone
- **WHEN** plugins are configured
- **THEN** `plugins/xr` and `XRModule` SHALL NOT exist, and no target SHALL link the legacy `RHI`/`RenderCore`

#### Scenario: PVS core survives
- **WHEN** the PVS plugin is configured
- **THEN** `PVS.Core` and `PVSTest` SHALL build without `RenderCore`, a render module, or a visualizer

#### Scenario: Legacy tools are gone
- **WHEN** the tools directory is inspected
- **THEN** `tools/noise` and `tools/perf` SHALL NOT exist, and the remaining tool targets SHALL build

### Requirement: Legacy-only third-party dependencies are removed

CMake SHALL NOT discover or link `boost`, `taskflow`, `glslang`, `SPIRV-Cross`, `OpenXR`, `gles`, or `implot`. Their find modules and any corresponding package declarations SHALL be removed. Unused `ktx`, `mimalloc`, and `cxxopts` find modules SHALL also be removed. `GKlib` and `metis` SHALL be retained (packages, find modules, and patches) for future meshlet integration, although no target SHALL link them yet.

#### Scenario: No legacy dependency is discovered
- **WHEN** a default build is configured
- **THEN** no removed dependency is searched for or referenced by a target

#### Scenario: Meshlet dependencies are retained
- **WHEN** `cmake/thirdparty.json` is inspected
- **THEN** it SHALL still declare `GKlib` and `metis`, reserved for future meshlet integration

#### Scenario: Necessary dependencies remain
- **WHEN** the Aurora and sandbox targets are built
- **THEN** their required dependencies (including `slang`, `dxcompiler`, `assimp`, `meshoptimizer`, `stb`, `nanosvg`, `freetype`, and image/compression libraries) SHALL remain available

### Requirement: Current documentation reflects the removal

Current build, plugin, and architecture documentation SHALL describe Aurora and the sandbox editor only, with no legacy render/shader/Qt-editor instructions. Archived OpenSpec changes SHALL remain historical records.

#### Scenario: Current docs have no legacy instructions
- **WHEN** current README, docs, and skill references are inspected
- **THEN** they SHALL NOT instruct users to enable `SKY_BUILD_LEGACY_RENDER` or `SKY_BUILD_EDITOR`

#### Scenario: Historical records are preserved
- **WHEN** `openspec/changes/archive/` is inspected
- **THEN** archived artifacts SHALL remain unchanged

### Requirement: SKY_DEVELOP 宏表达 develop 构建意图

`SKY_DEVELOP` SHALL 为一个 0/1 宏，默认 `0`；`SKY_DEVELOP == 1` SHALL 表示 develop/开发构建。未定义时 SHALL 经 `#ifndef` 兜底为 `0`。代码 SHALL 用 `#if SKY_DEVELOP` 判读。

#### Scenario: 默认关闭

- **WHEN** 未显式定义 `SKY_DEVELOP`
- **THEN** `SKY_DEVELOP == 0`

#### Scenario: develop 构建开启

- **WHEN** 构建系统开启 develop 模式（editor/tool 构建）
- **THEN** `SKY_DEVELOP == 1`

### Requirement: SKY_DEVELOP 与 _DEBUG 解耦

`SKY_DEVELOP`（构建意图）SHALL 与 `_DEBUG`（编译器 Debug 配置）正交：Debug 配置 SHALL NOT 隐含 `SKY_DEVELOP == 1`，`SKY_DEVELOP == 1` SHALL NOT 要求 Debug 配置。

#### Scenario: Release 编辑器可 develop

- **WHEN** Release 配置 + editor 构建
- **THEN** `SKY_DEVELOP == 1` 且 `_DEBUG` 未定义

### Requirement: 开发者设施统一挂 SKY_DEVELOP

开发者设施（资源名、调试 UI 等）SHALL 默认以 `SKY_DEVELOP` 为开关；`SKY_ENABLE_RESOURCE_NAME` 的 AUTO 策略 SHALL 判读 `SKY_DEVELOP`。资源命名 SHALL 允许显式 ON/OFF 覆盖；引擎断言与 GPU validation SHALL 使用独立配置策略，不能通过伪造 `_DEBUG` 改变 CRT。Develop 编译配置 SHALL 与 SKY_DEVELOP 设施意图保持区分。

#### Scenario: 资源名跟随 SKY_DEVELOP
- **WHEN** 资源命名策略为 AUTO 且 `SKY_DEVELOP == 1`
- **THEN** `SKY_ENABLE_RESOURCE_NAME == 1`；`SKY_DEVELOP == 0` 时默认关闭

#### Scenario: 显式关闭资源命名
- **WHEN** 编辑器使用 SKY_RESOURCE_NAMES=OFF
- **THEN** 资源命名关闭而其它开发设施和 CRT 配置不被改变

### Requirement: 平台选择

系统 SHALL 提供平台下拉选择，包含 `Win32`、`MacOS-x86`、`MacOS-arm`、`Linux` 四项。系统 SHALL 在启动时根据当前系统自动预选对应平台。

#### Scenario: 自动预选平台

- **WHEN** 界面启动且当前系统为 Windows
- **THEN** 平台下拉默认选中 `Win32`

#### Scenario: 自动预选 macOS 架构

- **WHEN** 界面启动且当前系统为 macOS
- **THEN** 根据 `platform.machine()` 选中 `MacOS-arm`（arm64）或 `MacOS-x86`（x86_64）

### Requirement: 路径配置

系统 SHALL 允许用户配置 Engine Root、Intermediate、Output 三个路径，均支持目录浏览选择。系统 SHALL 提供合理默认值。

#### Scenario: 选择目录

- **WHEN** 用户点击路径旁的浏览按钮并选择目录
- **THEN** 对应路径输入框更新为所选目录

### Requirement: 构建选项

系统 SHALL 提供 Jobs（整数，0 表示自动）、Force（布尔）、Clean（布尔）三个构建选项，并在构建时映射到 CLI 的 `-j`、`-f`、`-c` 参数。

#### Scenario: 选项映射

- **WHEN** 用户勾选 Force 并以 Jobs=8 发起构建
- **THEN** 系统以 `-j 8 -f` 参数调用 `third_party.py`

### Requirement: 包列表展示

系统 SHALL 读取 `cmake/thirdparty.json` 并展示包列表，按当前选中平台过滤（`platforms` 为空或缺失表示全平台）。系统 SHALL 展示包名、tag/commit、类型（header-only/static/tool）。

#### Scenario: 平台过滤

- **WHEN** 用户选择 `Win32` 平台
- **THEN** 表格仅展示 `platforms` 含 `Win32` 或未指定 `platforms` 的包

### Requirement: 已是最新状态

系统 SHALL 读取 `<output>/<platform>/build_metadata.json`，通过复刻 `compute_package_key` 的哈希逻辑，判断每个包是否已构建为最新，并在表格中展示「已是最新」状态。

#### Scenario: 已构建包标记

- **WHEN** 某包的 metadata `key` 与当前配置计算所得 key 一致且平台匹配
- **THEN** 该包在表格中显示「已是最新」标记

### Requirement: 构建全部与构建选中

系统 SHALL 提供「构建全部」与「构建选中」两种构建模式。构建全部不传 `-t`，构建选中传入 `-t <选中包名>`。

#### Scenario: 构建选中

- **WHEN** 用户在包表格中选中一个包并点击「构建选中」
- **THEN** 系统以 `-t <包名>` 参数调用 `third_party.py`

#### Scenario: 构建全部

- **WHEN** 用户点击「构建全部」
- **THEN** 系统不传 `-t` 参数调用 `third_party.py`

### Requirement: 实时日志

系统 SHALL 通过 QProcess 异步运行 CLI，并将 stdout/stderr 实时追加到只读日志视图，构建过程中 UI 不得阻塞。

#### Scenario: 流式输出

- **WHEN** CLI 在构建过程中输出日志
- **THEN** 日志视图实时追加该输出且界面保持可响应

### Requirement: 构建生命周期

系统 SHALL 在构建开始时禁用构建按钮、显示忙碌进度指示，在 `finished` 信号后根据退出码显示成功或失败，并支持取消（尽力回收进程树）。

#### Scenario: 构建成功

- **WHEN** CLI 进程以退出码 0 结束
- **THEN** 状态栏显示构建成功

#### Scenario: 构建失败

- **WHEN** CLI 进程以非 0 退出码结束
- **THEN** 状态栏显示构建失败

#### Scenario: 取消构建

- **WHEN** 用户点击取消按钮
- **THEN** 系统终止 CLI 进程并尽力回收其子进程树

### Requirement: 配置持久化

系统 SHALL 在关闭时保存 Engine Root、Intermediate、Output、平台、Jobs 配置，并在下次启动时恢复。

#### Scenario: 恢复配置

- **WHEN** 用户上次设置过路径与平台后关闭再重新启动
- **THEN** 界面恢复上次保存的路径与平台配置

### Requirement: 三方包 checkout 与配置版本一致

`third_party.py` SHALL 保证每次构建前，包的 checkout 与 `thirdparty.json` 配置的 `tag`/`commit` 一致——无论 checkout 是本次新克隆还是已存在。对已有浅克隆且配置 `tag` 的包，构建脚本 SHALL `fetch --depth=1 origin refs/tags/<tag>` 并 `checkout --detach FETCH_HEAD`。

#### Scenario: 已有 checkout 的 tag 升级

- **WHEN** `thirdparty.json` 中某包 tag 从 A 升到 B，且 intermediate 已存在 tag A 的浅克隆
- **THEN** 下次构建该包时 checkout 切换到 B（`git describe --tags` 为 B）

#### Scenario: 已有 checkout 的 tag 不变

- **WHEN** checkout 已在配置 tag 上
- **THEN** 构建正常进行（FETCH_HEAD 检出同一提交，幂等）

#### Scenario: submodule 包路径不受影响

- **WHEN** 包配置了 `submodule`/`submodules`（`need_full_history`）
- **THEN** 仍走 `fetch --tags` + `Branch_<tag>` 分支路径

### Requirement: 引擎代码与钉住版本 API 一致

引擎中对三方库的调用 SHALL 与 `thirdparty.json` 钉住的版本签名一致；版本不一致导致的编译错误 SHALL 通过修正 checkout 解决，SHALL NOT 通过改引擎代码向下适配陈旧版本。

#### Scenario: astcenc 5.7.0 调用

- **WHEN** `thirdparty.json` 钉住 astc `5.7.0`
- **THEN** `ImageCompressor.cpp` 以 4 参签名（含 `parent_context`）调用 `astcenc_context_alloc`,`AuroraCookTest` 全部通过

### Requirement: Explicit third-party output directory

The third-party build tool SHALL require an explicit output directory and SHALL NOT silently default to a path
inside the repository (`<engine>/build_3rd`).

#### Scenario: Output directory provided
- **WHEN** the tool is invoked with an output directory
- **THEN** the platform output tree SHALL be written there and the generated cmake cache SHALL point at it

#### Scenario: No implicit default
- **WHEN** no output directory is provided
- **THEN** the tool SHALL fail with a clear message rather than writing into the repository by default

### Requirement: No default archive or archive config writes

The tool SHALL NOT zip the output or write archive (file/MD5) information into a tracked configuration file by
default. `cmake/thirdparty.json` SHALL remain a dependency list without an `archives` section.

#### Scenario: Build does not modify tracked config
- **WHEN** a platform is built
- **THEN** no archive file SHALL be produced and `cmake/thirdparty.json` SHALL NOT be modified

#### Scenario: Optional packaging is explicit
- **WHEN** packaging is desired
- **THEN** it SHALL be an explicit, opt-in step that does not mutate tracked files

### Requirement: CMake consumes configuration, not archives

The CMake build SHALL obtain the third-party location from configuration (`3RD_PATH` / `SKY_THIRD_PARTY_*`) and
SHALL NOT read an `archives` entry.

#### Scenario: Configure with an explicit path
- **WHEN** the project is configured with the third-party path set
- **THEN** it SHALL locate the third-party packages without any `archives` entry in `thirdparty.json`

### Requirement: Generated state is not tracked

Per-platform build metadata and the cmake cache SHALL live under the output directory and SHALL NOT be tracked.

#### Scenario: Metadata stays outside the tree
- **WHEN** a package is built
- **THEN** its build metadata SHALL be written under the output directory and SHALL NOT appear as a tracked
  change

### Requirement: Production translation units have one compile owner

The build SHALL assign each production translation unit one primary target. Consumers SHALL reuse that target by linking a static or object library instead of recompiling its sources. The rule SHALL apply to direct target declarations, build wrappers, GLOB results and later target_sources additions. Tests SHALL NOT compile production sources merely because their owner is disabled. Publishing production translation units through interface sources SHALL fail configuration.

#### Scenario: Duplicate hidden in a glob
- **WHEN** a module implementation and another target include the same production source through different paths or GLOB variables
- **THEN** configuration SHALL fail with the source and target identities
- **AND** the diagnostic SHALL direct the consumer to link the implementation target

#### Scenario: Production owner disabled
- **WHEN** a test includes a production source while its normal target is disabled
- **THEN** configuration SHALL still reject that source in the test

#### Scenario: Legitimate object reuse
- **WHEN** multiple consumers link one object/static implementation target or reference its TARGET_OBJECTS
- **THEN** the build SHALL accept the single compilation owner

### Requirement: Exceptions preserve precise test contracts

The build SHALL permit shared test-only fixtures and explicitly declared compilation variants. A production-source variant SHALL identify an existing owner, one exact source, a reason and required distinct compile definitions; the consuming target SHALL be a test. Both targets SHALL actually compile that source, and the declared differences SHALL be present. Generated code SHALL remain subject to unique ownership, while HEADER_FILE_ONLY sources SHALL not count as compiled units.

#### Scenario: Static factory variant
- **WHEN** a test compiles the production module entry with a distinct static factory tag and declares its exact contract
- **THEN** configuration SHALL accept the variant
- **AND** removing the source or required definition SHALL make the declaration fail

#### Scenario: Shared test helper
- **WHEN** multiple test executables or DLL probes compile a helper located in a test-only directory
- **THEN** configuration SHALL accept that fixture without exempting production sources

### Requirement: Ownership validation is automatic and regression tested

The common build entry SHALL automatically validate the complete configured target graph after deferred source additions, including independent game projects. It SHALL normalize source identity and SHALL fail on source expressions it cannot validate. The guard SHALL have executable positive and negative CMake fixtures exposed through CTest and independent CI without engine third-party SDKs. It SHALL NOT provide a global bypass switch.

#### Scenario: Later source addition
- **WHEN** a later target_sources call or deferred call introduces cross-directory production source borrowing
- **THEN** the automatic final pass SHALL reject the configuration

#### Scenario: Game launcher generated registration
- **WHEN** an independent game project attaches explicitly generated registration from its build directory to the launcher
- **THEN** the ownership check SHALL accept that unique generated source

### Requirement: Engine animation integration remains independently selectable

The build SHALL declare SKY_BUILD_ANIMATION as an engine option defaulting to ON independently of plugins/plugins.json. The Animation algorithm library SHALL contain both the existing algorithms and pure Graph definition/evaluation code under engine/animation/core, depend only on Core and optional ACL, and remain available when this option is OFF. Its tests SHALL be unified in AnimationTest. Separate Animation.Graph.Static and Animation.GraphTest targets SHALL be removed. When ON, engine/animation SHALL provide separate GraphAssets (under assets) and World targets; Builder SHALL additionally require SKY_DEVELOP and Editor SHALL additionally require SKY_DEVELOP and SKY_BUILD_SANDBOX. Integration targets SHALL be configured after their engine dependencies and before host dependency binding. Each production source SHALL retain exactly one compilation owner.

#### Scenario: Animation integration enabled
- **WHEN** animation integration, developer facilities and Sandbox are enabled
- **THEN** Animation.GraphAssets, Animation.World, Animation.Builder and Animation.Editor implementation/module/test targets SHALL be configured from engine/animation with their existing names and public includes

#### Scenario: Algorithm without integration
- **WHEN** SKY_BUILD_ANIMATION is OFF
- **THEN** Animation SHALL include pure Graph support and remain buildable with AnimationTest, while GraphAssets, World, Builder and Editor integration targets SHALL be absent

#### Scenario: Engine option survives plugin manifest changes
- **WHEN** a build is configured with SKY_BUILD_ANIMATION=OFF and the plugin manifest subsequently changes
- **THEN** the explicit engine option SHALL remain OFF and ModuleManager::GetBuildOption SHALL report false for SKY_BUILD_ANIMATION rather than an unknown option

#### Scenario: Module packaging remains stable
- **WHEN** an enabled animation module is packaged statically or dynamically
- **THEN** its module name, metadata owner and runtime dependencies SHALL remain unchanged without an animation plugin registration
