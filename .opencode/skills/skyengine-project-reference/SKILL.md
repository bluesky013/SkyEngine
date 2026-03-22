---
name: skyengine-project-reference
description: Reference the SkyEngine project layout, build targets, module dependencies, and third-party library map when answering architecture or repository structure questions.
compatibility: opencode
metadata:
  source: README.md, repository inspection
  audience: contributors
---

# SkyEngine Project Reference

Use this skill when the task needs repository context instead of fresh exploration, especially for:

- explaining where a subsystem lives
- mapping target names to source directories
- recalling build flags and plugin toggles
- identifying third-party dependencies and their purpose
- summarizing render backends, asset pipeline, or module relationships

## Working rules

- Treat this skill as a quick-reference source for repository structure and build knowledge.
- Prefer this reference before doing broad repo searches when the user asks for high-level project understanding.
- If the request depends on current file contents beyond this document, verify with tools instead of assuming.
- Quote paths, targets, and flags exactly as listed here.

## SkyEngine 项目速查手册

## 概览

SkyEngine 是模块化游戏引擎，C++20，支持 Windows / macOS / Android / iOS / Linux。  
构建系统：CMake 3.10+，输出目录 `output/bin/`。

---

## 目录结构

```
SkyEngine/
├── engine/             # 引擎核心模块
│   ├── core/           # 基础库 (数学/内存/文件/日志/异步)
│   ├── framework/      # 应用框架 (窗口/输入/资产/世界)
│   ├── animation/      # 动画系统
│   ├── render/         # 渲染系统 (多后端)
│   ├── physics/        # 物理接口
│   ├── navigation/     # 导航接口
│   ├── editor/         # 编辑器 (Qt5)
│   ├── launcher/       # 运行时入口
│   └── test/           # 单元测试
├── plugins/            # 可选插件
├── tools/              # 开发工具
├── assets/             # 资源文件 (shader/material/texture)
├── configs/            # 运行时配置
├── cmake/              # 构建配置
├── python/             # Python 工具链
```

---

## 构建目标一览

### 引擎核心 (Static Libraries)

| Target | 目录 | 依赖 | 说明 |
|--------|------|------|------|
| `Core` | engine/core | sfmt, crc32, taskflow, boost, rapidjson | 基础库：数学/内存/文件/哈希/日志 |
| `Framework` | engine/framework | Core, SDL | 应用框架：窗口/输入/资产/世界 |
| `Animation` | engine/animation | Core | 骨骼动画/混合/播放 |
| `RHI` | engine/render/backend/rhi | Core | 渲染硬件抽象层 |
| `VulkanRHI` / `VulkanRHI.Static` | engine/render/backend/vulkan | RHI, vma | Vulkan 后端 |
| `DX12RHI` | engine/render/backend/dx12 | RHI | DX12 后端 (Win32) |
| `MetalRHI` | engine/render/backend/metal | RHI | Metal 后端 (Apple) |
| `RenderCore` | engine/render/core | Core, RHI, ShaderCompiler.Static | 渲染核心设施 |
| `RenderAdaptor` | engine/render/adaptor | RenderCore, Animation, ImGuiRender, Framework | 渲染适配层 |
| `ShaderCompiler.Static` | engine/render/shader | Core, RHI, glslang, SPIRVCross, dxcompiler | Shader 编译 |
| `RenderBuilder.Static` | engine/render/builder/render | Framework, ShaderCompiler.Static, stb, assimp, ispc_texcomp, meshoptimizer, metis | 资源构建 |
| `ImGuiRender` | engine/render/imgui | RenderCore, Framework, imgui | ImGui 渲染层 |
| `Physics` | engine/physics | Framework | 物理接口 |
| `Navigation` | engine/navigation | Framework | 导航接口 |
| `EditorFramework` | engine/editor/framework | Framework, RenderAdaptor, Physics, Qt5 | 编辑器框架 |

### 共享模块 (Shared Libraries)

| Target | 说明 |
|--------|------|
| `SkyRender` | 主渲染模块 |
| `SkyRender.Builder` | 渲染资源构建模块 |
| `SkyRender.Editor` | 编辑器渲染 (Qt5) |
| `ShaderCompiler` | Shader 编译器 (动态库) |
| `Terrain` / `TerrainEditor` | 地形系统 |

### 可执行文件

| Target | 说明 |
|--------|------|
| `Launcher` | 游戏运行时 |
| `Editor` | 编辑器 (Qt5) |
| `ShaderTool` | 命令行 shader 编译工具 |
| `AssetBuilder` | 资产构建工具 |
| `SkyImageViewer` | 纹理查看器 |

### 插件 (Shared Libraries)

| Target | 开关 | 依赖 |
|--------|------|------|
| `BulletPhysicsModule` | SKY_BUILD_BULLET | bullet3 |
| `RecastNavigation` | SKY_BUILD_RECAST | recast |
| `PythonModule` | SKY_BUILD_PYTHON | cpython |
| `CompressionModule` | SKY_BUILD_COMPRESSION | lz4 |
| `FreeTypeModule` | SKY_BUILD_FREETYPE | freetype |
| `ImGuizmoModule` | SKY_BUILD_GUIZMO + SKY_BUILD_EDITOR | ImGuizmo |
| `XRModule` | SKY_BUILD_XR | OpenXR |

### 插件编译配置管理

- `plugins/plugins.json` 是插件编译配置的统一入口
- 每条配置至少定义：`name`、`dir`、`cmake_var`、`enabled`，部分插件还带 `requires_editor`
- `cmake/plugin_switches.cmake` 在顶层配置阶段读取该文件，并把 `cmake_var` 生成为 CMake cache 变量
- `plugins/CMakeLists.txt` 再次读取该文件，仅当对应 `cmake_var` 为 ON 时才 `add_subdirectory(<dir>)`
- `requires_editor: true` 的插件还要求编辑器模式生效；当前 `guizmo` 走 `SKY_BUILD_GUIZMO`，并受 `SKY_BUILD_EDITOR` / `SKY_EDITOR` 约束
- `plugins/plugins.json` 管理的是编译/生成阶段是否纳入工程，不是运行时模块自动加载清单
- 运行时模块清单分别由 `configs/modules_game.json` 与 `configs/modules_editor.json` 管理

当前 `plugins/plugins.json` 默认项：

- `guizmo` → `SKY_BUILD_GUIZMO=ON`，`requires_editor=true`
- `python` → `SKY_BUILD_PYTHON=OFF`
- `xr` → `SKY_BUILD_XR=OFF`
- `bullet` → `SKY_BUILD_BULLET=ON`
- `recast` → `SKY_BUILD_RECAST=ON`
- `freetype` → `SKY_BUILD_FREETYPE=ON`
- `terrain` → `SKY_BUILD_TERRAIN=ON`
- `compression` → `SKY_BUILD_COMPRESSION=ON`
- `pvs` → `SKY_BUILD_PVS=ON`

### 测试

| Target | 说明 |
|--------|------|
| `CoreTest` | Core 模块测试 |
| `FrameworkTest` | Framework 模块测试 |
| `AnimationTest` | 动画测试 |
| `VulkanTest` | Vulkan 测试 |
| `RenderTest` | 渲染测试 |
| `RenderBuilderTest` | 构建器测试 |
| `TerrainTest` | 地形测试 |

---

## CMake 编译选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `SKY_BUILD_EDITOR` | OFF | 编辑器 (需要 Qt5) |
| `SKY_BUILD_TEST` | OFF | 单元测试 |
| `SKY_BUILD_TOOL` | OFF | 资产工具 |
| `SKY_BUILD_GLES` | OFF | OpenGL ES 后端 |
| `SKY_BUILD_XR` | OFF | OpenXR 支持 |
| `SKY_BUILD_PYTHON` | OFF | Python 脚本 |
| `SKY_BUILD_CPYTHON` | OFF | CPython 嵌入 |
| `SKY_BUILD_COMPRESSION` | OFF | LZ4 压缩 |
| `SKY_BUILD_FREETYPE` | OFF | 字体渲染 |
| `SKY_BUILD_BULLET` | OFF | Bullet 物理 |
| `SKY_BUILD_RECAST` | OFF | Recast 导航 |
| `SKY_BUILD_GUIZMO` | OFF | ImGuizmo 插件编译开关（仍需编辑器模式） |
| `SKY_BUILD_TERRAIN` | OFF | Terrain 插件编译开关 |
| `SKY_BUILD_PVS` | OFF | PVS 插件编译开关 |
| `SKY_USE_TRACY` | OFF | Tracy 性能分析 |
| `SKY_MATH_SIMD` | OFF | SIMD 数学优化 |

---

## 三方库速查

### 核心依赖 (始终需要)

| 包名 | 版本 | 类型 | 用途 |
|------|------|------|------|
| sfmt | 1.5.4 | static | 随机数生成 (SIMD) |
| boost | 1.88.0 | static | container, graph |
| taskflow | v3.7.0 | header-only | 任务并行调度 |
| rapidjson | v1.1.0 | header-only | JSON 解析 |
| sdl | 2.32.6 | static | 窗口/输入 (Win/Mac/Linux) |
| googletest | v1.17.0 | static | 单元测试 |

### 渲染依赖

| 包名 | 版本 | 类型 | 用途 |
|------|------|------|------|
| vma | v3.2.1 | header-only | Vulkan Memory Allocator |
| imgui | v1.88 | header-only | 即时模式 GUI |
| glslang | 15.3.0 | static | GLSL→SPIR-V 编译 |
| SPIRV-Cross | sdk-1.4.313.0 | static | SPIR-V 反射/转换 |
| dxcompiler | v1.8.2502 | static | HLSL→DXIL/SPIR-V (Win32) |

### 编辑器依赖 (SKY_BUILD_EDITOR)

| 包名 | 版本 | 类型 | 用途 |
|------|------|------|------|
| assimp | v6.0.2 | shared | 模型导入 |
| meshoptimizer | v0.23 | static | 网格优化/简化 |
| stb | (commit) | header-only | 图像加载 |
| ispc_texcomp | (custom) | shared (dll) | BC/ASTC 纹理压缩 (ISPC) |
| astc | 5.2.0 | static | ASTC 纹理压缩 (ARM) |
| GKlib | METIS-v5.1.1 | static | 图分区辅助 |
| metis | v5.2.1 | static | 网格分区 (meshlet) |
| ImGuizmo | 1.83 | header-only | 3D 变换 Gizmo |

### 可选插件依赖

| 包名 | 版本 | 开关 |
|------|------|------|
| bullet3 | 3.25 | SKY_BUILD_BULLET |
| recast | v1.6.0 | SKY_BUILD_RECAST |
| lz4 | v1.10.0 | SKY_BUILD_COMPRESSION |
| freetype | 2-13-3 | SKY_BUILD_FREETYPE |
| tracy | v0.11.1 | SKY_USE_TRACY |

### Patches 列表

需要应用 patch 的包：crc32, GKlib, glslang, imgui, ImGuizmo, metis, rapidjson, recast, sfmt, stb

---

## 模块加载配置

### 游戏模式 (configs/modules_game.json)
```
FreeTypeModule → depends: SkyRender
```

### 编辑器模式 (configs/modules_editor.json)
```
ImGuizmoModule       → depends: SkyRender.Editor
FreeTypeModule       → depends: SkyRender.Editor
BulletPhysicsModule  → depends: SkyRender.Editor
CompressionModule    → depends: (none)
```

---

## 资源系统

### Asset Build Presets (configs/asset_build_presets.json)
```
bundles: common, tex_pc, tex_mobile
presets:
  windows → common + tex_pc
  macos   → common + tex_pc
  ios     → common + tex_mobile
```

### 渲染预加载 (configs/render_preload_assets.json)
- gui.tech, text.tech, post_processing.tech, depth.tech
- depth_resolve.tech, depth_downsample.tech, debug.tech
- skybox.tech, meshlet_debug.tech, brdf_lut.tech

### Shader 列表 (assets/shaders/)
```
顶层: box, brdf_lut, debug, depth*, draw_id, error, skybox, standard_pbr
子目录: color/, common/, depth/, layout/, lighting/, mesh/,
       pipeline/, post_processing/, terrain/, ui/, vertex/
```

### 材质 (assets/materials/)
- default_terrain.mat, skybox.mat, standard_pbr.mat
- standard_transparent_pbr.mat, unlit.mat, volume_simple.mat

---

## 渲染后端

| 后端 | 平台 | 状态 |
|------|------|------|
| Vulkan | Win32, macOS, Android | 主要后端 |
| DX12 | Win32 | 支持 |
| Metal | macOS, iOS | 支持 |
| GLES | Android | 可选 (SKY_BUILD_GLES) |

---

## Python 工具链

| 脚本 | 说明 |
|------|------|
| `python/third_party.py` | 三方库编译 |
| `python/setup.py` | 引擎配置 UI |
| `python/project_manager.py` | 项目管理 UI |
| `python/build/Presets.py` | CMake Preset 生成 |
| `python/build/Project.py` | 项目配置/构建 |

### 三方库编译命令
```bash
# 列出所有包
python python/third_party.py -e . -p Win32 --list

# 全量编译 (Win32)
python python/third_party.py -i <intermediate> -o <output> -e . -p Win32

# 编译单个包
python python/third_party.py -i <intermediate> -o <output> -e . -p Win32 -t <package>

# 强制重建
python python/third_party.py -i <intermediate> -o <output> -e . -p Win32 -f
```

---

## 纹理压缩流水线

### 当前方案
- **BC 格式** (PC): ispc_texcomp (BC1/BC3/BC4/BC5/BC6H/BC7) — 预编译 DLL
- **ASTC 格式** (Mobile): astc-encoder (ARM 官方库) — 静态库
- **资源分包**: tex_pc (BC) / tex_mobile (ASTC)
- **构建入口**: RenderBuilder.Static → 依赖 ispc_texcomp + astc

### 使用者
- `RenderBuilder.Static` 链接 `3rdParty::ispc_texcomp`
- `Findispc_texcomp.cmake` 提供 DLL (Win32) / dylib (macOS)

---

## 依赖关系图 (简化)

```
Core ← Framework ← RenderAdaptor ← Launcher
                 ← Physics       ← BulletPhysicsModule
                 ← Navigation    ← RecastNavigation
                 ← EditorFramework ← Editor

RHI ← VulkanRHI / DX12RHI / MetalRHI
RHI ← RenderCore ← RenderAdaptor
    ← ShaderCompiler.Static ← RenderBuilder.Static
                             ← SkyRender.Builder
```
