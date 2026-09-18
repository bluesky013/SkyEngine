## Why

slang 的 `SLANG_ENABLE_DXIL=ON` 会触发 `include(FetchDXC)`，从 GitHub 下载 DXC（`v1.9.2602` 的 `dxc_2026_02_20.zip`）供 DXIL 编译使用。这带来两个问题：

1. **网络依赖**：全新 bootstrap（或 `rm -rf` build 目录）时，FetchContent 会重新下载 ~50MB 的 DXC zip，在无网/慢网环境下会卡死（本环境已实测 300KB/30min 基本停滞）。原构建能过只是因为 `_deps` 缓存恰好存在。
2. **重复的 DXC 来源**：引擎自身已有 `dxcompiler` 第三方包（`thirdparty.json`，源码构建，产出 `dxcompiler.dll` + `dxil.dll`，供 aurora 运行时 DXIL 使用）。slang 又单独下载一份 DXC，两份 DXC 并存，来源不一致。

版本已对齐：`dxcompiler` 包已 bump 到 `v1.9.2602`（与 slang `FetchDXC` 锁定的 DXC 版本一致）。因此可以让 slang 复用引擎的 `dxcompiler` 包，彻底消除网络下载。

## What Changes

- **patch slang 构建**（`cmake/patches/slang.patch` 追加一个 hunk）：把 `CMakeLists.txt` 里的
  ```cmake
  if(SLANG_ENABLE_DXIL)
      include(FetchDXC)
  endif()
  ```
  替换为「从 `${3RD_PATH}/dxcompiler/bin` 拷贝 `dxcompiler.dll` + `dxil.dll` 到 slang 构建输出 bin」，不再 `include(FetchDXC)`（不再下载）。
- **路径合规**：使用 `${3RD_PATH}`（由 `third_party.py` 注入的 CMake 变量），不写死本地全路径（遵守 coding rules Rule 7）。
- **前置已就绪**：`dxcompiler`（包序号 13）先于 `slang`（包序号 28）构建，故 slang configure 时 `${3RD_PATH}/dxcompiler/bin` 已存在；版本已对齐 `v1.9.2602`。

## Capabilities

### New Capabilities

- `aurora-slang-dxc`: slang 的 DXIL 依赖引擎 `dxcompiler` 包（`3RD_PATH/dxcompiler`），SHALL NOT 从网络下载 DXC，SHALL NOT 写死本地全路径。

### Modified Capabilities

（无 —— 不改运行时能力，仅改 slang 构建依赖来源。）

## Impact

- **修改文件**：`cmake/patches/slang.patch`（追加一个 hunk 替换 `include(FetchDXC)`）。
- **重建**：slang 包重新 bootstrap + 编译（无 glslang/spirv-tools 后较快，且不再触发 DXC 下载）。
- **验证**：`AuroraShaderTest` 的 `CompileDxilForDx12` / `AuroraD3D12Test.*` 仍绿。
- **依赖**：`dxcompiler` 包（`build_3rd/Win32/dxcompiler/bin/dxcompiler.dll` + `dxil.dll`，版本 `v1.9.2602`）。
- **不影响**：运行时 API / shader 模块接口；旧 `engine/shader/ShaderCompilerDXC`（仍消费 `dxcompiler` 包，版本向后兼容）。
