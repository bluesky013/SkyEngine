## Context

slang 的 DXIL 支持由 `SLANG_ENABLE_DXIL=ON` 打开，`CMakeLists.txt` 里：

```cmake
if(SLANG_ENABLE_DXIL)
    include(FetchDXC)
endif()
```

`FetchDXC.cmake` 用 FetchContent 从 GitHub 下载预编译 DXC（`v1.9.2602`），把 `dxcompiler.dll` + `dxil.dll` 拷到 slang 构建输出 `bin/`（`copy-dxcompiler` / `copy-dxil` target）。

引擎自身已有 `dxcompiler` 第三方包（`thirdparty.json`，源码构建 `DirectXShaderCompiler`，产出 `dxcompiler.dll` + `dxil.dll`），由旧 `engine/shader/ShaderCompilerDXC` 与 aurora 运行时 DXIL 消费。该包已 bump 到 `v1.9.2602`（与 slang `FetchDXC` 版本对齐，见上一个 change）。

关键约束：

- `third_party.py` 的 `fill_common_options` 会给 slang 的 cmake configure 注入 `-D3RD_PATH=<build_3rd>/<platform>`（指向 `build_3rd/Win32`）。
- 包构建顺序：`dxcompiler`（序号 13）先于 `slang`（序号 28），故 slang configure 时 `${3RD_PATH}/dxcompiler/bin` 已存在。
- `cmake/patches/slang.patch` 由 `third_party.py` 在 `git reset --hard` + `git clean -f` 后 `git apply`，是 slang 构建的唯一改动通道。
- coding rules Rule 7：编译系统禁止写死本地全路径，必须用 CMake 变量 / `3RD_PATH` / 相对路径。

## Goals / Non-Goals

**Goals:**

- slang 的 DXIL 复用引擎 `dxcompiler` 包（`${3RD_PATH}/dxcompiler/bin`），不再 `include(FetchDXC)`、不再网络下载。
- patch 只用 `${3RD_PATH}`（CMake 变量），不写死本地全路径。

**Non-Goals:**

- 不改 slang 运行时/编译 API（DXIL 编译行为不变）。
- 不合并 `dxcompiler` 与 slang 包（两个包保留，只改 slang 侧 DXC 来源）。
- 不处理 MacOS/Linux 的 DXC（`dxcompiler` 包 `platforms: ["Win32"]`；非 Win32 平台 slang 的 `SLANG_ENABLE_DXIL` 本就随平台关闭或不适用）。

## Decisions

### D1: patch `CMakeLists.txt` 替换 `include(FetchDXC)`，不改 `FetchDXC.cmake`

在 `cmake/patches/slang.patch` 追加一个 hunk，把：

```cmake
if(SLANG_ENABLE_DXIL)
    include(FetchDXC)
endif()
```

替换为直接从引擎 `dxcompiler` 包拷贝：

```cmake
if(SLANG_ENABLE_DXIL)
    # Reuse the engine's dxcompiler package (3RD_PATH) instead of downloading.
    set(_sky_dxc_bin "${3RD_PATH}/dxcompiler/bin")
    foreach(_dll dxcompiler dxil)
        set(_dst "${CMAKE_BINARY_DIR}/$<CONFIG>/bin/${_dll}.dll")
        add_custom_command(
            OUTPUT "${_dst}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_sky_dxc_bin}/${_dll}.dll" "${_dst}"
            DEPENDS "${_sky_dxc_bin}/${_dll}.dll"
            VERBATIM
        )
        add_custom_target(copy-${_dll} DEPENDS "${_dst}")
        set_target_properties(copy-${_dll} PROPERTIES FOLDER generated)
    endforeach()
endif()
```

- **理由**：`FetchDXC.cmake` 的职责是「拷贝 dxcompiler.dll + dxil.dll 到构建 bin」，下载只是取源；把源换成 `${3RD_PATH}/dxcompiler/bin` 即可，无需改动 FetchContent 逻辑（直接绕开）。保留 `copy-dxcompiler` / `copy-dxil` target 名，兼容 slang 内部对该 target 的引用。
- **备选**：patch `FetchDXC.cmake` 内部用 `${3RD_PATH}` —— 被否，FetchContent 的 `URL` 字段不适合直接指本地目录，改动更侵入。
- **备选**：让 aurora 运行时直接靠引擎 `dxcompiler` 包、slang 构建完全不拷贝 DXC —— 需要验证 slang 构建/tests 是否依赖 `copy-*` target 与 build bin 里的 DXC DLL，风险更高，先按「替换源」落地。

### D2: 只支持 Win32（与 `dxcompiler` 包平台一致）

patch 的 `foreach` 拷贝假设 `${3RD_PATH}/dxcompiler/bin/*.dll` 存在，这仅在 Win32 成立（`dxcompiler` 包 `platforms: ["Win32"]`）。非 Win32 平台 `SLANG_ENABLE_DXIL` 本就关闭或走各自路径，patch 不影响它们。

- **理由**：引擎 `dxcompiler` 只在 Win32 提供；slang 的 DXIL 目标也只在 Windows 有意义（DX12 是 Windows 专属）。
- **备选**：跨平台 DXC 拷贝 —— 被否，无对应包来源。

### D3: 版本对齐已前置

`dxcompiler` 包 tag 已 bump 到 `v1.9.2602`，与 slang `FetchDXC` 锁定的 DXC 版本一致（上一个 change 完成）。本 change 不做版本改动，只改「来源」。

- **理由**：版本不一致会导致 slang 对 DXIL 的假设（SM 6.5、DXIL 特性）与引擎 DXC 产出不匹配；对齐后才能安全复用。

## Risks / Trade-offs

- **[patch 后 slang 构建需验证]** 替换 `include(FetchDXC)` 后，需确认 slang 编译 DXIL 目标（`CompileDxilForDx12` / `AuroraD3D12Test.*`）仍绿。→ 缓解：tasks 含「重建 slang + 跑 AuroraShaderTest」。
- **[slang 内部对 `copy-*` target 的引用]** slang 可能有别的 target 依赖 `copy-dxcompiler`/`copy-dxil`。→ 缓解：保留同名 target，patch 只改源，不改 target 名。
- **[`3RD_PATH` 变量名假设]** 依赖 `third_party.py` 注入的 `-D3RD_PATH`。→ 缓解：已核实 `fill_common_options` 注入该变量；若 CMake 未定义则 `add_custom_command` 的 `DEPENDS` 会失败，属显式错误（好于静默下载）。
- **[Win32 硬编码 bin 路径]** 拷贝目的 `$<CONFIG>/bin/` 与源 `${3RD_PATH}/dxcompiler/bin` 都是相对 CMake 变量/结构，无本机全路径，合规。

## Migration Plan

1. 追加 `cmake/patches/slang.patch` hunk。
2. 重建 slang（`python third_party.py -p Win32 -t slang -f`，不再触发 DXC 下载）。
3. 重建 `AuroraShaderTest` 并跑 DXIL 相关测试。
4. archive。

## Open Questions

- 非 Win32（Metal/MacOS）的 DXIL 需求：目前无，`SLANG_ENABLE_DXIL` 在 Mac 上关闭（Metal 走 MSL），后续如需 DXIL-on-Mac 再评估。
- slang 构建是否可完全去掉 DXC 拷贝（依赖引擎运行时 DXC）——待 patch 落地后验证，作为可选精简。
