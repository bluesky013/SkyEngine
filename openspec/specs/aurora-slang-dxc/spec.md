# aurora-slang-dxc Specification

## Purpose
TBD - created by archiving change aurora-slang-reuse-dxc. Update Purpose after archive.
## Requirements
### Requirement: slang DXIL 复用引擎 dxcompiler 包

`SLANG_ENABLE_DXIL=ON` 时，slang 构建 SHALL 从 `${3RD_PATH}/dxcompiler/bin` 拷贝 `dxcompiler.dll` 与 `dxil.dll` 到 slang 构建输出 bin（`copy-dxcompiler` / `copy-dxil` target），SHALL NOT `include(FetchDXC)` 从网络下载 DXC。

slang 构建 SHALL 用 `${3RD_PATH}`（`third_party.py` 注入的 CMake 变量）定位 `dxcompiler` 包，SHALL NOT 写死本地全路径（遵守 coding rules Rule 7）。

#### Scenario: 不触发网络下载

- **WHEN** 全新 bootstrap（`build_3rd/intermediate/slang` 无 `_deps/dxc-*` 缓存）构建 slang
- **THEN** configure 阶段不出现 `Downloading DXC from https://...`，不执行 FetchContent 下载

#### Scenario: 复用引擎 DXC

- **WHEN** slang 构建的 DXIL 编译路径解析 `dxcompiler.dll` / `dxil.dll`
- **THEN** 来源为 `${3RD_PATH}/dxcompiler/bin`（引擎 `dxcompiler` 包，版本 `v1.9.2602`）

#### Scenario: 无本地全路径

- **WHEN** 阅读 `cmake/patches/slang.patch` 中 DXIL 相关 hunk
- **THEN** 不含盘符/主目录前缀的绝对路径，仅用 `${3RD_PATH}` / `${CMAKE_BINARY_DIR}` 等 CMake 变量

#### Scenario: DXIL 编译仍可用

- **WHEN** 重建后运行 `AuroraShaderTest` 的 `CompileDxilForDx12` 与 `AuroraD3D12Test.*`
- **THEN** 全部通过，DXIL 产物与反射结果不变

#### Scenario: aurora shader 模块声明 dxcompiler 运行时依赖

- **WHEN** Windows 上构建 `Aurora.Shader` / `AuroraShaderTest`
- **THEN** `3rdParty::dxcompiler` 的 `dxcompiler.dll` + `dxil.dll` 被自动拷贝到运行时输出目录（`output/bin/Debug`），无需手动拷贝；DXIL 编译在干净环境也能找到这两个 DLL

### Requirement: slang patch 兼容 Xcode generator

slang patch（`cmake/patches/slang.patch`）SHALL 保证 slang 在 Xcode generator 下 configure/generate 成功。patch 引入或保留的 per-source `COMPILE_OPTIONS` SHALL NOT 包含 `$<CONFIG:...>` 等 per-config generator 表达式；需要按配置区分行为时 SHALL 在 configure 期按编译器分支（如 `if(MSVC)`），仅在 Visual Studio generator 使用的分支内使用 per-config genex。

#### Scenario: Xcode configure 成功

- **WHEN** `python third_party.py -p MacOS-x86 -t slang -f` 构建 slang
- **THEN** configure/generate 阶段不出现 `Xcode does not support per-config per-source COMPILE_OPTIONS`，Debug + Release 编译安装成功

#### Scenario: MSVC 语义不变

- **WHEN** Win32（Visual Studio generator）构建 slang
- **THEN** `slang-rich-diagnostics.cpp` 在 Debug 下跳过 size 优化，非 Debug 使用 `/O1;/Os`，与上游行为一致

#### Scenario: GCC/Clang 全配置 size 优化

- **WHEN** Xcode / Ninja 等 generator 下用 Clang 或 GCC 构建 slang
- **THEN** `slang-rich-diagnostics.cpp` 在所有配置下分别使用 `-Oz`（Clang）/ `-Os`（GCC）

