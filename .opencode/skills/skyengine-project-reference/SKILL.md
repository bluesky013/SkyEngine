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
| `Core` | engine/core | sfmt, rapidjson | 基础库：数学/内存/文件/哈希/日志 |
| `Framework` | engine/framework | Core | 应用框架：窗口/输入/资产/世界 |
| `Animation` | engine/animation | Core | 骨骼动画/混合/播放 |
| `Aurora.RHI.Static` | engine/aurora/rhi/interface | Core | Aurora RHI 接口 |
| `Aurora.RHI` backends | engine/aurora/rhi/{vulkan,dx12,metal} | Aurora.RHI.Static | Aurora 后端 |
| `Aurora.Core` | engine/aurora/core | Core, Aurora.RHI.Static | Aurora 资源/场景/ECS |
| `Aurora.Pipeline` | engine/aurora/pipeline | Aurora.RHI.Static | Aurora RDG 管线 |
| `Aurora.ShaderCompiler.Static` | engine/aurora/shader | Core, Aurora.RHI.Static, slang | Slang Shader 编译 |
| `AuroraCook.Static` | engine/aurora/cook | Aurora.Core | 离线资产构建 |
| `Aurora.Adaptor` | engine/aurora/adaptor | Aurora.Core, Framework | Aurora 运行时对接 |
| `Physics` | engine/physics | Framework | 物理接口 |
| `Navigation` | engine/navigation | Framework | 导航接口 |

### 共享模块 (Shared Libraries)

| Target | 说明 |
|--------|------|
| `AuroraRender` | Aurora 运行时渲染模块 |
| `Aurora.ShaderCompiler` | Slang Shader 编译器模块 |
| `AuroraCook` | Aurora 离线资产构建模块 |
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
| `JoltPhysicsModule` | SKY_BUILD_JOLT | jolt |
| `RecastNavigation` | SKY_BUILD_RECAST | recast |
| `PythonModule` | SKY_BUILD_PYTHON | cpython |
| `CompressionModule` | SKY_BUILD_COMPRESSION | lz4 |
| `FreeTypeModule` | SKY_BUILD_FREETYPE | freetype |

### 插件编译配置管理

- `plugins/plugins.json` 是插件编译配置的统一入口
- 每条配置至少定义：`name`、`dir`、`cmake_var`、`enabled`，部分插件还带 `requires_editor`
- `cmake/plugin_switches.cmake` 在顶层配置阶段读取该文件，并把 `cmake_var` 生成为 CMake cache 变量
- `plugins/CMakeLists.txt` 再次读取该文件，仅当对应 `cmake_var` 为 ON 时才 `add_subdirectory(<dir>)`
- `requires_editor: true` 的插件要求 sandbox 编辑器模式生效（`SKY_BUILD_SANDBOX=ON`）
- `plugins/plugins.json` 管理的是编译/生成阶段是否纳入工程，不是运行时模块自动加载清单
- 运行时模块清单分别由 `configs/modules_game.json` 与 `configs/modules_editor.json` 管理

当前 `plugins/plugins.json` 默认项：

- `python` → `SKY_BUILD_PYTHON=OFF`
- `jolt` → `SKY_BUILD_JOLT=ON`
- `recast` → `SKY_BUILD_RECAST=ON`
- `vegetation` → `SKY_BUILD_VEGETATION=ON`
- `terrain` → `SKY_BUILD_TERRAIN=ON`
- `compression` → `SKY_BUILD_COMPRESSION=ON`
- `pvs` → `SKY_BUILD_PVS=ON`

### 测试

| Target | 说明 |
|--------|------|
| `CoreTest` | Core 模块测试 |
| `FrameworkTest` | Framework 模块测试 |
| `AnimationTest` | 动画测试 |
| `Aurora.RHITest` | Aurora RHI 测试 |
| `Aurora.CoreTest` | Aurora Core 测试 |
| `Aurora.ShaderTest` | Aurora Shader 测试 |
| `Aurora.PipelineTest` | Aurora Pipeline 测试 |
| `Aurora.AdaptorTest` | Aurora Adaptor 测试 |
| `Aurora.CookTest` | Aurora Cook 测试 |
| `PVSTest` | PVS Core 测试 |
| `TerrainTest` | 地形测试 |

---

## CMake 编译选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `SKY_BUILD_SANDBOX` | OFF | Sandbox 编辑器 |
| `SKY_BUILD_TEST` | OFF | 单元测试 |
| `SKY_BUILD_TOOL` | OFF | 资产工具 |
| `SKY_BUILD_PYTHON` | OFF | Python 脚本 |
| `SKY_BUILD_CPYTHON` | OFF | CPython 嵌入 |
| `SKY_BUILD_COMPRESSION` | OFF | LZ4 压缩 |
| `SKY_BUILD_FREETYPE` | OFF | 字体渲染 |
| `SKY_BUILD_JOLT` | OFF | Jolt 物理 |
| `SKY_BUILD_RECAST` | OFF | Recast 导航 |
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
| rapidjson | v1.1.0 | header-only | JSON 解析 |
| googletest | v1.17.0 | static | 单元测试 |

### 渲染依赖

| 包名 | 版本 | 类型 | 用途 |
|------|------|------|------|
| vma | v3.2.1 | header-only | Vulkan Memory Allocator |
| dxcompiler | v1.9.2602 | static | HLSL→DXIL (Win32, Aurora) |
| slang | v2026.9.2 | static | Aurora Slang Shader 编译 |

### Cook / 编辑器依赖

| 包名 | 版本 | 类型 | 用途 |
|------|------|------|------|
| assimp | v6.0.2 | shared | 模型导入 |
| meshoptimizer | v0.23 | static | 网格优化/简化 |
| stb | (commit) | header-only | 图像加载 |
| ispc_texcomp | (custom) | shared (dll) | BC/ASTC 纹理压缩 (ISPC) |
| astc | 5.2.0 | static | ASTC 纹理压缩 (ARM) |
| GKlib | METIS-v5.1.1 | static | 保留：后续 meshlet 图分区 |
| metis | v5.2.1 | static | 保留：后续 meshlet 网格分区 |

### 可选插件依赖

| 包名 | 版本 | 开关 |
|------|------|------|
| jolt | 5.6.0 | SKY_BUILD_JOLT |
| recast | v1.6.0 | SKY_BUILD_RECAST |
| lz4 | v1.10.0 | SKY_BUILD_COMPRESSION |
| freetype | 2-13-3 | SKY_BUILD_FREETYPE |
| tracy | v0.11.1 | SKY_USE_TRACY |

### Patches 列表

需要应用 patch 的包：crc32, GKlib, metis, rapidjson, recast, sfmt, stb

---

## 模块加载配置

### 游戏模式 (configs/modules_game.json)
```
Aurora.ShaderCompiler / AudioModule / RecastNavigation / JoltPhysicsModule
JoltPhysicsRenderModule / TerrainRender / VegetationRender
```

### 编辑器模式 (configs/modules_editor.json)
```
Aurora.ShaderCompiler / Aurora.Cook / Aurora.Render.Editor
Content.Views / SandboxModule / JoltPhysicsModule / RecastNavigation
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

### Shader 资源
- Aurora 根：`engine/aurora/assets/shaders/`；内置程序为 `aurora/materials/{Unlit,Lit,Shadow}.slang` 和 `aurora/pipeline/ToneMap.slang`。
- 每个插件独立：`plugins/<owner>/assets/shaders/`；虚拟相对路径以 `<owner>/` 开头，跨模块唯一。
- 模块资源描述统一提供 inventory、include roots、依赖和 exports；`engine/aurora/resources.json` 是内置示例。
- C++ 直接包含权威 `.h`；`ShaderHeaders.Update` 导出并持久化 `.slangh`，`ShaderHeaders.Check` 检查同步。
- 开发部署用 `ShaderResources.Stage`；项目打包经统一资源图 cook，并按目标聚合 shader product index。

### 材质
- `.mat` 使用 version-2 数据模型：`techniques[].program.vertex/fragment` 各自显式指定 canonical source 和 entry。
- Scene 按 queue Tag 选择 technique；材质与 Feature 的 Batch 输入显式分配 binding 所有权。
- 旧 HLSL 材质、`.vtxlib` 和 `.tech` 预加载清单不再作为当前工作流。完整示例见 `docs/aurora/shader-design-and-migration.md`。

---

## 渲染后端

| 后端 | 平台 | 状态 |
|------|------|------|
| Vulkan | Win32, macOS, Android | 主要后端 |
| DX12 | Win32 | 支持 |
| Metal | macOS, iOS | 支持 |

---

## Python 工具链

| 脚本 | 说明 |
|------|------|
| `python/third_party.py` | 三方库编译 |

### 三方库编译命令
```bash
# 列出所有包
python python/third_party.py -p Win32 --list

# 全量编译到共享目录（中间目录默认位于输出根目录下）
python python/third_party.py -p Win32 -o ../sky_3rd

# 指定三方输出目录
python python/third_party.py -i <intermediate> -o <output> -e . -p Win32

# 编译单个包
python python/third_party.py -i <intermediate> -o <output> -e . -p Win32 -t <package>

# 强制重建
python python/third_party.py -i <intermediate> -o <output> -e . -p Win32 -f
```

- 未指定 `-e/--engine` 时，默认使用当前引擎根目录
- `-o/--output` 必须显式指定；`-i/--intermediate` 默认使用 `<output>/intermediate/`
- 当前平台完整 `3RD_PATH` 为 `<output>/<platform>/`；兼容的引擎 checkout 可共享仓库外的输出根目录
- 每次成功构建后会刷新 `<output>/thirdparty_cache_<platform>.cmake`
- 外部输出通过 `-D3RD_PATH=<output>/<platform>` 或本地 CMake preset 接入；仅旧 `build_3rd` 路径保留自动加载兼容

---

## 纹理压缩流水线

### 当前方案
- **BC 格式** (PC): ispc_texcomp (BC1/BC3/BC4/BC5/BC6H/BC7) — 预编译 DLL
- **ASTC 格式** (Mobile): astc-encoder (ARM 官方库) — 静态库
- **资源分包**: tex_pc (BC) / tex_mobile (ASTC)
- **构建入口**: AuroraCook.Static → 依赖 ispc_texcomp + astc

### 使用者
- `AuroraCook.Static` 链接 `3rdParty::ispc_texcomp`
- `Findispc_texcomp.cmake` 提供 DLL (Win32) / dylib (macOS)

---

## 依赖关系图 (简化)

```
Core ← Framework ← Aurora.Adaptor ← Launcher
                 ← Physics       ← JoltPhysicsModule
                 ← Navigation    ← RecastNavigation
                 ← SandboxModule ← SandboxEditor

Aurora.RHI.Static ← Aurora.RHI {Vulkan,DX12,Metal}
Aurora.RHI.Static ← Aurora.Core ← Aurora.Adaptor
Aurora.ShaderCompiler.Static ← Aurora.Pipeline
```
