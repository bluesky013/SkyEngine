## Context

slang 上游 `source/slang/CMakeLists.txt` 对 `slang-rich-diagnostics.cpp` 设置：

```cmake
string(CONCAT RICH_DIAG_SIZE_OPT_FLAGS
    "$<$<NOT:$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>>:"
    "$<IF:$<CXX_COMPILER_ID:MSVC>,/O1;/Os,$<IF:$<CXX_COMPILER_ID:GNU>,-Os,-Oz>>>")
set_source_files_properties(slang-rich-diagnostics.cpp PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_OPTIONS "${RICH_DIAG_SIZE_OPT_FLAGS}")
```

目的：该文件包含生成的 fiddle 代码，体积大，默认按 size 优化（MSVC `/O1;/Os`，Clang `-Oz`，GCC `-Os`）；MSVC Debug 下跳过以避免与 `/RTC1` 冲突。

关键约束：

- `third_party.py` 的 `tool_chain` 把 `MacOS-x86` / `MacOS-arm` / `IOS` 映射到 Xcode generator，Win32 映射 Visual Studio（支持 per-config genex）。
- Xcode generator 不支持 per-source `COMPILE_OPTIONS` 中出现 `$<CONFIG:...>`（CMake 官方限制，generate 期直接报错）。
- `cmake/patches/slang.patch` 是 slang 构建的唯一改动通道（`git reset --hard` 后 `git apply`），且 patch 由 checkout 内 `git diff` 再生成。

## Goals / Non-Goals

**Goals:**

- slang 在 Xcode generator 下 configure/generate 通过，`MacOS-x86` 的 Debug + Release 全量构建成功。
- 保持上游语义：MSVC Debug 跳过 size 优化，GCC/Clang 全配置 size 优化。

**Non-Goals:**

- 不改 `slang-rich-diagnostics.cpp` 本身，也不关闭 size 优化。
- 不向上游提 PR（patch 仅引擎内维护）。

## Decisions

### D1: 按编译器分支改写，而不是删除优化或换 generator

把单个 genex 表达式拆为 `if(MSVC) / elseif(GNU) / else()` 三支：

- MSVC 分支保留 `$<$<NOT:$<CONFIG:Debug>>>:/O1;/Os>`——只在 Visual Studio generator 下求值，per-config genex 合法。
- GNU/Clang 分支用纯字面量 `-Os` / `-Oz`——上游对 GCC/Clang 本就全配置生效，`$<CONFIG>` 条件只对 MSVC 有意义，去掉后语义不变且 Xcode 可接受。

备选方案：

- 改用 Ninja/Makefile generator 构建 slang：偏离引擎 `third_party.py` 的平台映射约定，影响面大。
- 直接删掉该优化：Release 二进制体积回退，且丢失上游修复意图。
