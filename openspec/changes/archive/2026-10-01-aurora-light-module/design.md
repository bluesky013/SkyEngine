## Context

`aurora/core` 现有 light 相关代码分散：

- `scene/SceneTypes.h`：`LightType`、`Light`、`MainLight`（此前被建模为结果快照，挂在 `RenderScene` 成员上）。
- `renderer/LightSystem.h/.cpp`：`LightRenderData`、`LocalLightSystem`、`ExtractLightPosition/Direction`（上一 change `aurora-main-light` 引入）。
- 数学/几何基础已在 `core`：`GeometryStreams` + `GenerateSphere/Cone`（`core/math/GeometryGenerator.h`）、`AABB` / `BoundingBoxSphere`、`SceneView`（view/viewProject + frustum）。

目标：把 light 收拢为 `aurora/core` 内的独立子模块（目录分组，非新 target），并补齐供「光体积保守光栅化」使用的 CPU 核心处理函数。渲染主循环 / GPU pass 尚未实施（`aurora-renderer`），所以本 change 只产出可单测的纯 CPU 函数与几何，不接 GPU。

## Goals / Non-Goals

**Goals:**

- `include/aurora/light/` + `src/light/` 子模块；类型与服务全部归位，`Aurora.Core.Static` 通过 GLOB 自动纳入。
- `MainLight` 建模为 ECS 组件（主方向光标记），`RenderScene` 不再持有主光成员。
- `LocalLightSystem` 收集 `Light+WorldInfo` 为调用方持有的 `LightRenderData`。
- 提供影响体积分类、世界保守包围、shell 凸包几何、屏幕保守 2D 包围盒、屏幕保守 tile 覆盖五类核心函数。
- `SceneTypes.h` 转发 light 类型，现有 include/调用方不受影响（除头路径）。
- 纯 CPU、无 RHI 依赖、可单测。

**Non-Goals:**

- GPU 光体积 pass / stencil / PSO / shader、阴影壳、tile/cluster GPU 分派。
- 局域光 GPU buffer 上传（`LightRenderData` 仅保留结构）。
- mesh/LOD 迁移；物理光照着色。

## Decisions

### D1: 子模块布局与迁移

**决策**

```
engine/aurora/core/
  include/aurora/light/
    LightTypes.h     // LightType, Light, MainLight, LightRenderData (+ Light 的 SKY_TYPE_TAG)
    LightSystem.h    // LocalLightSystem, ExtractLightPosition/Direction
    LightShell.h     // 体积/包围/shell/覆盖 API
  src/light/
    LightSystem.cpp
    LightShell.cpp
```

删除 `include/aurora/renderer/LightSystem.h`、`src/renderer/LightSystem.cpp`。`scene/SceneTypes.h` `#include <aurora/light/LightTypes.h>` 并移除本地 light 定义。

**理由**：与既有 `shader/` 子模块（`include/aurora/shader` + `src/aurora/shader`）同源做法：目录分组、无独立 target。**替代方案**：新建 `Aurora.Core.Light` target——被否（用户要求“非 lib”，且会引入 target 依赖管理成本）。

### D2: `MainLight` 是 ECS 组件，`LightRenderData` 是收集结果

**决策**：`MainLight { bool castShadow = true; }` 作为 ECS 组件声明于 `LightTypes.h` 并注册 `SKY_TYPE_TAG`；挂在承载主方向光的 entity 上（与 `Light{DIRECTIONAL}` + `WorldInfo` 同实体），作为「主光」的权威标记与主光专属数据（阴影等）。`RenderScene` **不**持有 `MainLight` 成员（删除 `Get/SetMainLight`）。`LightRenderData` 是 `LocalLightSystem` 收集产出的 GPU 数据记录，非组件。

**理由**：ECS 里 light 状态应落在实体上；主光是 authored 意图（哪个方向光是 key light），不该由系统按 intensity 隐式选出后塞进 scene。**替代方案**：(a) 系统按 intensity 选主光写回 scene——被否，破坏 ECS 且引入 scene 缓存；(b) 收集结果写回组件——被否，收集是每帧派生的 GPU 数据，不是 authored 状态。

### D3: 影响体积分类 `LightVolume`

**决策**

```cpp
enum class LightVolumeType : uint8_t { NONE, SPHERE, CONE, BOX };
struct LightVolume {
    LightVolumeType type = LightVolumeType::NONE;
    // SPHERE (point)
    Vector3 center; float radius;
    // CONE (spot)
    Vector3 apex; Vector3 axis; float height; float baseRadius;
    // BOX (directional)
    AABB box;
    // 共享：世界保守包围
    AABB bounds;
};
LightVolume ComputeLightVolume(const Light&, const WorldInfo&, const SceneView* view);
```

- `DIRECTIONAL → BOX`：有 `view` 时用 `Inverse(viewProject)` 取视锥 8 角点世界 AABB；无 view 时用大默认盒。
- `POINT → SPHERE`：center=派生位置，radius=`range`。
- `SPOT → CONE`：apex=派生位置，axis=派生朝向，height=`range`，baseRadius=`range * tan(outerConeAngle)`。
- `bounds` 恒为体积的世界 AABB（保守）。

**理由**：统一入口便于后续 pass/tile 分派复用；`view` 可空以便纯几何单测。**替代方案**：每个类型独立工厂函数——被否，调用方需先分类。

### D4: 保守策略——AABB 投影 + w<=0 兜底

**决策**：`ComputeConservativeScreenRect(volume, viewProj, extent)` 取 `volume.bounds` 的 8 角点投影，屏幕坐标取 min/max；若任一角点 `clip.w <= 0`（跨近平面/相机后）或体积类型为 `BOX`，SHALL 返回覆盖整屏的保守矩形（`valid=false` 或满 extent）。`ComputeConservativeCoverage` 对 shell 三角形做 tile 级保守覆盖：三角形屏幕 AABB 与 tile 相交即标记（保守过近似）。

**理由**：保守性优先于紧致；跨近平面的精确裁剪复杂且本 change 非目标。**替代方案**：精确视锥裁剪——后续 change。

### D5: `GenerateLightShellGeometry`

**决策**：返回 `sky::GeometryStreams`（与 `BuiltinGeometry`/几何生成器一致）。

- SPHERE：`GenerateSphere(radius, rings, sectors)` + 平移 center。
- CONE：专用生成（apex 在原点，底面环中心 = `axis*height`，半径 `baseRadius`），避免 `GenerateCone` 的居中/朝向再变换。
- BOX：`GenerateCube` 缩放到 `box` 尺寸 + 平移中心。

**理由**：直接产出保守光栅化光体积所需的凸包几何，后续可经 `BuiltinGeometry::Build` 上传。**替代方案**：只输出参数化描述——被否，用户明确要 shell 几何。

### D6: `SceneView` 依赖与分层

**决策**：`light/` 只依赖 `Core` + aurora 自身资源/数学；`ComputeLightVolume` 对 `SceneView`（`scene/SceneView.h`，同属 core）取指针，可为空。不依赖 pipeline / adaptor / rhi。

**理由**：满足 `engine` 分层；`LightShell` 保持纯数学，便于测试。

## Risks / Trade-offs

- [方向光的 BOX 语义（视锥盒 vs 无限）易误解] → 文档化：`view==nullptr` 用大默认盒；屏幕函数对 BOX 直接返回整屏。
- [保守 tile 覆盖可能高估] → 接受（保守优先）；单测断言“至少覆盖真实覆盖”。
- [头路径 BREAKING] → `SceneTypes.h` 转发 + 全仓 include 更新；编译验证 adaptor。
- [与 `aurora-main-light` 的 spec delta 重叠] → `aurora-pipeline` MODIFIED 需求写成含字段变更的期望终态；建议 main-light 先归档。

## Migration Plan

1. 新建 `light/` 头与实现，迁移类型/服务。
2. `SceneTypes.h` 改为 include 转发；删旧 `renderer/LightSystem.*`。
3. 更新 include（adaptor / tests）。
4. 重新 CMake configure（GLOB 变更）→ 构建 `Aurora.CoreTest` + `Aurora.Adaptor.Static`。
回滚：整体 revert。

## Open Questions

- 方向光 shell 是否应是「整屏全屏三角」而非盒（GPU 阶段再定）。
- tile 尺寸默认值（暂定 16x16，函数参数化）。
- `LightRenderData` 字段是否随局域光 change 调整（本 change 不动）。
