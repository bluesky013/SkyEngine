## Why

Light 相关代码目前散落在 `aurora/core` 两处：类型（`LightType` / `Light` / `MainLight`）在 `scene/SceneTypes.h`，服务（`LocalLightSystem`）在 `renderer/LightSystem.h`。随着光照处理增长（光源收集、变换派生、影响体积、shell 生成、屏幕保守包围/覆盖），需要把 light 归拢为独立子模块；同时 `MainLight` 此前被错误地做成 `RenderScene` 上的收集结果，应改为 ECS 组件。

## What Changes

- 新增 `aurora/core` 的 **light 子模块**（非 lib，仍编入 `Aurora.Core.Static`）：`include/aurora/light/` + `src/light/`。
- 迁移类型到 `aurora/light/LightTypes.h`：`LightType` / `Light` / `MainLight` / `LightRenderData`；`scene/SceneTypes.h` 改为 include 转发。
- **`MainLight` 改为 ECS 组件**（注册 `SKY_TYPE_TAG`）：挂在承载主方向光的 entity 上（与 `Light{DIRECTIONAL}` + `WorldInfo` 同实体），携带主光专属字段（`castShadow`）。**删除** `RenderScene` 的 `MainLight` 成员与 `Get/SetMainLight`。
- `LocalLightSystem` 职责改为**收集**：遍历 `View<Light, WorldInfo>`，把每个光源的位置/朝向/参数打包为 `LightRenderData` 追加到**调用方持有**的输出（renderer 侧 buffer），不在 scene 上缓存。
- 迁移服务到 `light/`：`LocalLightSystem`（原 `renderer/LightSystem.*` 删除）、`ExtractLightPosition/Direction`。
- 新增 `light/LightShell.h/.cpp` 核心处理：`ComputeLightVolume`（点=球/聚光=锥/平行光=盒）、`ComputeLightBounds`（保守 AABB）、`GenerateLightShellGeometry`（凸包 shell）、`ComputeConservativeScreenRect`（保守 2D 矩形）、`ComputeConservativeCoverage`（tile 保守覆盖）。
- **BREAKING**（aurora 内部）：`#include <aurora/renderer/LightSystem.h>` → `<aurora/light/LightSystem.h>`；`MainLight` 语义从 scene 成员变为组件。

## Capabilities

### New Capabilities

- `aurora-light`: light 子模块布局、light 组件（`Light` + `MainLight`）、`LocalLightSystem` 收集、变换派生、影响体积、世界/屏幕保守包围、shell 几何、屏幕保守覆盖。

### Modified Capabilities

- `aurora-pipeline`: `RenderScene（aurora）` 需求的场景组件列表更新（`Light` 声明位置改为 `aurora/light/LightTypes.h`；新增 `MainLight` 组件；`RenderScene` 不再持有主光）。

## Non-goals

- 光体积 GPU pass / stencil / PSO / shader（依赖未实施的 `aurora-renderer`）。
- 阴影贴图、shadow shell、PCF/PCSS。
- 局域光 GPU 上传与 tile/cluster GPU 分派（本 change 只做 CPU 收集与核心处理）。
- 物理光照着色。
- mesh / LOD（`StaticMeshSystem` 等）不迁移。

## Impact

- `engine/aurora/core`：新增 `include/aurora/light/`、`src/light/`；修改 `scene/SceneTypes.h`、`scene/RenderScene.h`；删除 `include/aurora/renderer/LightSystem.h`、`src/renderer/LightSystem.cpp`；`test/SceneCollectTest.cpp` 增补。
- `engine/aurora/adaptor`：`AuroraReflection.cpp` 经 `SceneTypes.h` 转发可见 `Light`（无需改）。
- CMake：无新 target（`GLOB_RECURSE` 自动纳入）。
- 与 change `aurora-main-light` 的 spec delta 重叠：`aurora-pipeline` 的 MODIFIED 需求已含 main-light 的字段变更；建议按 main-light → light-module 顺序归档。
