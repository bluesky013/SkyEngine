## ADDED Requirements

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
