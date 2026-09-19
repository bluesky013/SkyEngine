## Why

MacOS-x86 平台用 Xcode generator 构建 slang 时，CMake generate 阶段直接失败：

```
CMake Error in source/slang/CMakeLists.txt:
  Xcode does not support per-config per-source COMPILE_OPTIONS:
    $<$<NOT:$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>>:...>
  specified for source: slang-rich-diagnostics.cpp
```

slang 上游为压缩 `slang-rich-diagnostics.cpp`（含生成 fiddle 代码，体积大）的体积，对单源文件设置了带 `$<CONFIG:Debug>` 的 `COMPILE_OPTIONS`。Xcode generator 不支持 per-config 的 per-source 编译选项，导致 `MacOS-x86` / `MacOS-arm` / `IOS`（`third_party.py` 中均映射 Xcode）三个平台全部无法 configure slang。

## What Changes

- **patch slang 构建**（`cmake/patches/slang.patch` 追加一个 hunk）：把 `source/slang/CMakeLists.txt` 中基于 `$<CONFIG>` 单表达式版本改为按编译器分支：
  - `MSVC`：保留 `$<$<NOT:$<CONFIG:Debug>>:/O1;/Os>`（Visual Studio generator 支持 per-config genex，语义与上游一致）。
  - `GNU`：`-Os`；其余（Clang/AppleClang）：`-Oz`。这两个分支不含 `$<CONFIG>`，Xcode 可接受，且 GCC/Clang 本就全配置生效，语义不变。

## Capabilities

### New Capabilities

（无 —— 属于 `aurora-slang-dxc` 能力的补丁约束补充。）

### Modified Capabilities

- `aurora-slang-dxc`: 补充 slang patch 的 Xcode generator 限制——per-source `COMPILE_OPTIONS` SHALL NOT 含 `$<CONFIG>`。

## Impact

- **修改文件**：`cmake/patches/slang.patch`（追加一个 hunk 改写 `RICH_DIAG_SIZE_OPT_FLAGS`）。
- **重建**：`python third_party.py -p MacOS-x86 -t slang -f` 全量重建通过（Debug + Release），产物归档 `thirdparty_MacOS-x86_e97d2deae540.zip`。
- **不影响**：Windows（MSVC 分支语义与上游一致）；slang 运行时/编译 API。
