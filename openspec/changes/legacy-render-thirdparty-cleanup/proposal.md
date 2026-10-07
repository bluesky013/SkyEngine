# Change: legacy-render-thirdparty-cleanup

> 状态：Debt record（未实现，not scheduled）。渲染重构完成后执行的三方库清理清单；未排期，阻塞于 legacy render 退役。

## Why

以下三方库当前**仅**被 legacy `engine/render/**` 与 `engine/shader`（旧渲染/着色器）使用，在渲染重构（迁移到 aurora）完成后应完整清除，以缩小依赖面与构建时间：

| 库 | 当前使用者（仅 legacy） | 证据 |
|---|---|---|
| `taskflow` | `engine/render/core`（`RenderGraphContext.h`）、`engine/shader`（`ShaderFileSystem.h`） | `tf::Executor` |
| `boost` | `engine/render/core`（`render/rdg/*`、`render/mesh/MeshLodProxy.h`）、`engine/render/builder/render`（`ClusterBuilder.cpp`）、`engine/shader/src`（`ShaderCompiler.cpp`/`ShaderVariant.cpp`） | `boost/...` |
| `glslang` | `engine/shader/src`（`ShaderCompiler.cpp`/`ShaderCompilerGlsl.cpp`） | `glslang` |
| `SPIRV-Cross` | `engine/shader/src`（`ShaderCompiler.cpp`/`ShaderCross.cpp`） | `spirv_cross` |

> 注：`dxcompiler` 同时被新 `engine/aurora/shader` 使用 → **保留**。`assimp`/`meshoptimizer`/`ispc_texcomp`/`stb` 被新 `engine/aurora/cook` 使用 → **保留**。

`taskflow` 的 core 侧已在 `migrate-taskflow-to-threadpool` 中迁移（`Task`/`NamedThread` 基于 `ThreadPool`），链接已下移到 legacy 目标。

## What Changes（渲染重构后）

- 删除 legacy `engine/render/**` 与 `engine/shader`（或其 taskflow/boost/glslang/SPIRV-Cross 使用点）。
- 移除 CMake 链接：`engine/shader`（`3rdParty::taskflow`/`boost`/`glslang`/`SPIRVCross`）、`engine/render/core`（`taskflow`/`boost`）、`engine/render/builder/render`（`boost`）。
- 移除依赖声明：`cmake/thirdparty.cmake` 的 `sky_find_3rd(taskflow/boost/glslang/SPIRVCross)`；`cmake/thirdparty.json` 对应条目。

## Tasks（待渲染重构完成后）

- [ ] 1.1 确认 legacy `engine/render/**`、`engine/shader` 已无生产引用（被 aurora 取代）
- [ ] 1.2 移除 `taskflow`：CMake 链接 + `thirdparty.{cmake,json}` 条目（承接 `migrate-taskflow-to-threadpool` 2.2）
- [ ] 1.3 移除 `boost`
- [ ] 1.4 移除 `glslang`
- [ ] 1.5 移除 `SPIRV-Cross`
- [ ] 1.6 全量构建 + 测试通过，且 `grep -r "boost/\|glslang\|spirv_cross\|taskflow/"` 无残留

## Impact

- 删除第三方包（4 个）及其 third-party 构建
- 依赖方向更清（消除 legacy render/shader 的反向依赖面）
