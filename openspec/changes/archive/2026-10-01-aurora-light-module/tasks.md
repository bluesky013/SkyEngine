## 1. light 子模块与组件

- [x] 1.1 新建 `include/aurora/light/LightTypes.h`：`LightType` / `Light` / `MainLight`（组件 + `SKY_TYPE_TAG`）/ `LightRenderData`
- [x] 1.2 新建 `include/aurora/light/LightSystem.h`：`LocalLightSystem::Process(RenderScene&, std::vector<LightRenderData>&)` + `ExtractLightPosition/Direction`
- [x] 1.3 新建 `src/light/LightSystem.cpp`
- [x] 1.4 删除 `include/aurora/renderer/LightSystem.h` 与 `src/renderer/LightSystem.cpp`
- [x] 1.5 `scene/SceneTypes.h`：改为 `#include <aurora/light/LightTypes.h>` 转发，移除本地 light 定义与 `Light` tag
- [x] 1.6 `scene/RenderScene.h`：删除 `MainLight` 成员与 `Get/SetMainLight`

## 2. LocalLightSystem 收集

- [x] 2.1 `Process` 遍历 `View<Light, WorldInfo>`，按 `position/color/direction/params` 打包 `LightRenderData` 追加到调用方 `out`

## 3. LightShell 核心处理

- [x] 3.1 新建 `include/aurora/light/LightShell.h`：`LightVolumeType` / `LightVolume` / `ScreenRect` / `LightCoverage` 与函数声明
- [x] 3.2 `ComputeLightBounds`：点=球 AABB；锥=AABB 含 apex + 底面圆盘；平行光=大默认盒
- [x] 3.3 `ComputeLightVolume`：按类型分类，派生位置/朝向；`DIRECTIONAL` 用 `Inverse(viewProject)` 视锥角点求盒
- [x] 3.4 `GenerateLightShellGeometry`：SPHERE/CONE/BOX 凸包 shell；NONE 空
- [x] 3.5 `ComputeConservativeScreenRect`：投影 `bounds` 8 角点取 min/max；`w<=0` 或 `BOX` 整屏
- [x] 3.6 `ComputeConservativeCoverage`：shell 三角形 tile 级保守覆盖
- [x] 3.7 新建 `src/light/LightShell.cpp` 实现

## 4. 引用更新

- [x] 4.1 `SceneCollectTest.cpp` include 改为 `<aurora/light/LightSystem.h>` / `<aurora/light/LightShell.h>`
- [x] 4.2 确认 `AuroraReflection.cpp` 经 `SceneTypes.h` 转发可见 `Light`

## 5. 测试

- [x] 5.1 `MainLight` 是注册组件（Add/Get + `View<MainLight, Light, WorldInfo>`）
- [x] 5.2 `LocalLightSystem::Process` 收集 `LightRenderData`（点/聚光打包、调用方持有追加）
- [x] 5.3 变换派生（位置 / 朝向）
- [x] 5.4 体积分类（点=球、聚光=锥）
- [x] 5.5 世界保守包围（球 / 锥）
- [x] 5.6 shell 几何（球面 / 锥 apex+底面）
- [x] 5.7 屏幕保守矩形（屏内 + `BOX` 整屏）
- [x] 5.8 tile 保守覆盖（含中心 tile；屏外为空）

## 6. 构建与验证

- [x] 6.1 重新 CMake configure（GLOB 变更）
- [x] 6.2 构建并运行 `Aurora.CoreTest`，全部通过
- [x] 6.3 构建 `Aurora.Adaptor.Static`，确认无回归
