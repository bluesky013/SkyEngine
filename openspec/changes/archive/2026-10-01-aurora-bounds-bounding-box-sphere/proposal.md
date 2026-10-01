## Why

Aurora 的 scene 包围盒与 mesh/geometry 包围盒统一用 `AABB`（min/max），但渲染侧需要「盒 + 球」两种表达（球用于快速剔除与 LOD/排序）。`core/shapes/Bounds.h` 已提供 `BoundingBoxSphere`（center/extent/radius + `ToAABB()`）；`LodGroup` 已使用它。本 change 把 scene `Bounds` 与 mesh/geometry/sub-mesh/asset 的包围盒从 `AABB` 换成 `BoundingBoxSphere`，统一类型。

## What Changes

- scene `Bounds.worldBounds`：`AABB` → `BoundingBoxSphere`。
- `SceneView::FrustumCulling`：参数 `AABB` → `BoundingBoxSphere`（内部用 `Min()/Max()` 做 box-vs-frustum）。
- `RenderGeometry::localBounds` / `SetLocalBounds` / `GetLocalBounds`：`AABB` → `BoundingBoxSphere`。
- `Mesh::GetLocalBounds` 与 `SubMesh.bounds`：`AABB` → `BoundingBoxSphere`。
- `BuiltinGeometry::ComputeBounds`：返回 `BoundingBoxSphere::FromMinMax`。
- `MeshAssetData.bounds`：`AABB` → `BoundingBoxSphere`；序列化 `center/extent/radius`，`CURRENT_VERSION` 1 → 2（旧 v1 资产按版本守卫拒绝）。
- adaptor 反射：注册 `BoundingBoxSphere{center,extent,radius}`（替代 `AABB`）。
- bullet render：`TriangleMesh::AddView` 传 `data.bounds.ToAABB()`。
- **BREAKING**：`aurora-mesh` / `aurora-pipeline` / `aurora-resource` 相关接口/类型变更。

## Capabilities

### Modified Capabilities

- `aurora-mesh`: `SubMesh.bounds` 与 `Mesh::GetLocalBounds` 类型改为 `BoundingBoxSphere`。
- `aurora-pipeline`: `SceneView::FrustumCulling` 签名与 scene `Bounds` 组件类型。
- `aurora-resource`: `RenderGeometry.localBounds` 类型与本地包围盒需求措辞。

## Non-goals

- 不改 terrain/vegetation 自有的 `AABB` 场/单元包围盒。
- 不为旧 `MeshAssetData` v1 提供迁移读路径（按版本守卫拒绝，需重新烘焙）。

## Impact

- 代码：`aurora/core`（SceneTypes/SceneView/RenderGeometry/Mesh/BuiltinGeometry + 测试）、`aurora/adaptor`（MeshAsset/AuroraReflection）、`plugins/bullet`（render）。
- 资产：mesh 资产二进制布局变更（v1 → v2），需重烘焙。
