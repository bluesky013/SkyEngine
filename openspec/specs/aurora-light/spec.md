# aurora-light Specification

## Purpose
TBD - created by archiving change aurora-light-module. Update Purpose after archive.
## Requirements
### Requirement: Light 子模块布局与类型归属

light 类型与服务 SHALL 位于 `aurora/core` 的 light 子模块（非独立 target，编入 `Aurora.Core.Static`）：

- `aurora/light/LightTypes.h` SHALL 声明 `LightType` / `Light` / `MainLight` / `LightRenderData`，并包含 `Light` 与 `MainLight` 的 `SKY_TYPE_TAG`。
- `aurora/light/LightSystem.h` SHALL 声明 `LocalLightSystem` 与 `ExtractLightPosition` / `ExtractLightDirection`。
- `aurora/light/LightShell.h` SHALL 声明影响体积 / shell / 覆盖处理函数。
- `aurora/scene/SceneTypes.h` SHALL 通过 include `aurora/light/LightTypes.h` 转发 light 类型，不再本地声明。

#### Scenario: 头转发保持兼容
- **WHEN** 仅 `#include <aurora/scene/SceneTypes.h>` 后使用 `sky::aurora::Light`
- **THEN** 编译通过（类型经转发可见）

#### Scenario: 组件类型可注册
- **WHEN** 对 `MainLight` 调用 `TypeId<MainLight>()`
- **THEN** 编译通过（已注册 `SKY_TYPE_TAG`）

### Requirement: MainLight 主方向光组件

`MainLight` SHALL 是注册的 ECS 组件，携带主光专属数据（至少 `bool castShadow`，默认 `true`），用于把某个 `Light{DIRECTIONAL}` entity 标记为场景主光；其颜色 / 强度取自同实体的 `Light`，世界朝向取自同实体的 `WorldInfo`。`RenderScene` SHALL NOT 持有或缓存主光（无 `Get/SetMainLight`）。

#### Scenario: 主光作为组件挂载
- **WHEN** `scene.Add<MainLight>(id, MainLight{})` 且该 entity 有 `Light` / `WorldInfo`
- **THEN** `scene.Get<MainLight>(id)` 非空；`View<MainLight, Light, WorldInfo>` 迭代到该 entity

#### Scenario: RenderScene 不持有主光
- **WHEN** 检查 `RenderScene` 接口
- **THEN** 不存在 `MainLight` 成员或 `Get/SetMainLight` 访问器

### Requirement: LocalLightSystem 收集 LightRenderData

`LocalLightSystem::Process(RenderScene&, std::vector<LightRenderData>& out)` SHALL 遍历 `View<Light, WorldInfo>`，对每个光源派生世界位置 / 朝向并打包 `LightRenderData` 追加到 `out`；`out` SHALL 由调用方持有（不在 scene 上缓存）。打包 SHALL 为：`position = (pos, type)`、`color = (rgb, intensity)`、`direction = (dir, 0)`、`params = (range, innerConeAngle, outerConeAngle, 0)`。`Process` SHALL 无状态。

#### Scenario: 点光打包
- **WHEN** entity 有 `Light{type=POINT, color, intensity, range}` 与平移 `(1,2,3)` 的 `WorldInfo`
- **THEN** `out` 追加记录 `position == (1,2,3, POINT)`、`color.w == intensity`、`params.x == range`

#### Scenario: 聚光方向
- **WHEN** entity 的 `WorldInfo` 为 Identity（派生朝向 -Z）
- **THEN** 追加记录的 `direction.xyz` 约为 `(0,0,-1)`

#### Scenario: 调用方持有缓冲
- **WHEN** 对同一 scene 连续调用两次 `Process`
- **THEN** `out` 追加两轮记录（不覆盖）

#### Scenario: 无光源
- **WHEN** scene 中没有 `Light` 组件
- **THEN** `out` 不变

### Requirement: 光源位置与朝向由实体变换派生

`ExtractLightPosition(const Matrix4&)` SHALL 返回矩阵平移列 `m[3].xyz`；`ExtractLightDirection(const Matrix4&)` SHALL 返回归一化后的矩阵旋转作用于局部前向 `Vector3::VEC3_NZ`（退化矩阵回退为 `VEC3_NZ`）。

#### Scenario: 位置来自平移列
- **WHEN** `world.m[3] == (10,20,0,1)`
- **THEN** `ExtractLightPosition` 返回 `(10,20,0)`

#### Scenario: 朝向来自旋转
- **WHEN** `world` 为绕 Y 轴 90° 的纯旋转
- **THEN** `ExtractLightDirection` 返回 `-Z` 经该旋转后的单位向量

### Requirement: LightVolume 影响体积分类

`ComputeLightVolume(const Light&, const WorldInfo&, const SceneView *view)` SHALL 按 `LightType` 分类：`DIRECTIONAL → BOX`、`POINT → SPHERE`、`SPOT → CONE`，并用 `WorldInfo` 派生位置 / 朝向。`POINT` 半径为 `range`；`SPOT` 的 `apex` 为派生位置、`axis` 为派生朝向、`height` 为 `range`、`baseRadius` 为 `range * tan(outerConeAngle)`；`DIRECTIONAL` 盒在 `view` 非空时覆盖视锥世界 AABB，`view` 为空时用大默认盒。

#### Scenario: 点光体积
- **WHEN** `Light{type=POINT, range=7}`、平移 `(1,2,3)`
- **THEN** `type == SPHERE`、`center == (1,2,3)`、`radius == 7`

#### Scenario: 聚光体积
- **WHEN** `Light{type=SPOT, range=10, outerConeAngle=0.5}`、朝向 -Z
- **THEN** `type == CONE`、`axis` 为单位 -Z、`baseRadius == 10 * tan(0.5)`

#### Scenario: 平行光体积
- **WHEN** `Light{type=DIRECTIONAL}` 且 `view != nullptr`
- **THEN** `type == BOX`，盒覆盖视锥世界 AABB

### Requirement: 世界空间保守包围

`ComputeLightBounds(const Light&, const WorldInfo&)` SHALL 返回包含光源影响体积的世界空间保守 AABB：点光为 `position ± range`；聚光同时包含 apex 与底面圆盘（底面轴向外扩 `baseRadius * sqrt(1 - axis_i^2)`）；平行光为大默认盒。`LightVolume::bounds`（球 / 锥）SHALL 与之一致。

#### Scenario: 球包围
- **WHEN** 点光位置 `(0,0,0)`、`range=5`
- **THEN** `min == (-5,-5,-5)`、`max == (5,5,5)`

#### Scenario: 锥包围包含底面
- **WHEN** 聚光 apex `(0,0,0)`、axis `-Y`、`height=4`、`baseRadius≈2`
- **THEN** `bounds` 覆盖 apex 与底面圆盘

### Requirement: Light shell 凸包几何

`GenerateLightShellGeometry(const LightVolume&, sectors, rings)` SHALL 返回 `sky::GeometryStreams` 凸包 shell：`SPHERE` 为球心 `center`、半径 `radius` 的球面；`CONE` 的 apex 位于 `volume.apex`、底面环中心位于 `apex + axis*height`、底面半径 `baseRadius`；`BOX` 由 `box` 缩放/平移的立方体。`NONE` SHALL 返回空 streams。

#### Scenario: 球 shell 顶点在球面
- **WHEN** `SPHERE{center=(0,0,0), radius=2}`
- **THEN** 每个 position 到原点距离约为 2

#### Scenario: 锥 shell apex 与底面
- **WHEN** `CONE{apex=(0,0,0), axis=(0,-1,0), height=4, baseRadius=2}`
- **THEN** 存在顶点在原点；底面环顶点 y 约为 -4 且到轴距离约为 2

### Requirement: 屏幕空间保守 2D 包围盒

`ComputeConservativeScreenRect(const LightVolume&, const Matrix4 &viewProj, const Vector2 &extent)` SHALL 投影 `bounds` 8 角点取屏幕 min/max；任一角点 `clip.w <= 0` 或体积为 `BOX` 时 SHALL 返回覆盖整屏的保守矩形（`valid == false`）。

#### Scenario: 屏内体积保守覆盖
- **WHEN** 位于相机前方的体积投影到屏幕
- **THEN** 返回矩形包含其真实投影（允许更大），`valid == true`

#### Scenario: 平行光整屏
- **WHEN** `volume.type == BOX`
- **THEN** 返回矩形覆盖 `[0,extent]` 且 `valid == false`

### Requirement: 屏幕空间保守 tile 覆盖

`ComputeConservativeCoverage(const LightVolume&, viewProj, extent, tileSize)` SHALL 对 shell 三角形做 tile 粒度保守光栅化：三角形屏幕 AABB 与 tile 相交即标记；返回 `tilesX` / `tilesY` 与覆盖 tile 线性索引列表，且 SHALL 为真覆盖的超集。`BOX` 或任一 shell 顶点在相机后时 SHALL 覆盖全部 tile。

#### Scenario: 覆盖为真覆盖超集
- **WHEN** 体积投影覆盖屏幕中心
- **THEN** 返回 tile 集合包含中心 tile

#### Scenario: 屏幕外体积空覆盖
- **WHEN** 体积完全在屏幕外
- **THEN** 返回覆盖列表为空

