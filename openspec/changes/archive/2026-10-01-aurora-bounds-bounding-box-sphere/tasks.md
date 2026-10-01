## 1. scene bounds

- [x] 1.1 `Bounds.worldBounds` 从 `AABB` 改为 `BoundingBoxSphere`
- [x] 1.2 `SceneView::FrustumCulling(const BoundingBoxSphere&)`（内部用 `Min()/Max()`）
- [x] 1.3 更新 `SceneCollectTest`（改用 `BoundingBoxSphere`）

## 2. mesh / geometry bounds

- [x] 2.1 `RenderGeometry.localBounds` + `Set/GetLocalBounds` 改 `BoundingBoxSphere`
- [x] 2.2 `Mesh::GetLocalBounds` + `SubMesh.bounds` 改 `BoundingBoxSphere`
- [x] 2.3 `BuiltinGeometry::ComputeBounds` 返回 `BoundingBoxSphere::FromMinMax`
- [x] 2.4 更新 `RenderGeometryTest` / `BuiltinGeometryTest`（`Max()/Min()`）

## 3. asset / adaptor / plugin

- [x] 3.1 `MeshAssetData.bounds` 改 `BoundingBoxSphere`；序列化 `center/extent/radius`；`CURRENT_VERSION` 1→2
- [x] 3.2 `AuroraReflection` 注册 `BoundingBoxSphere{center,extent,radius}`
- [x] 3.3 bullet render `TriangleMesh::AddView` 传 `data.bounds.ToAABB()`

## 4. 验证与收尾

- [x] 4.1 构建 `Aurora.CoreTest` / `Aurora.PipelineTest` / `Aurora.AdaptorTest` + `BulletPhysicsRenderModule`
- [x] 4.2 `ctest` 相关用例通过
- [x] 4.3 clang-format
- [ ] 4.4 待用户确认后 `openspec archive aurora-bounds-bounding-box-sphere`
