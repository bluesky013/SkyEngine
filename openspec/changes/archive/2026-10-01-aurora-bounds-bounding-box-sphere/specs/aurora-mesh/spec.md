## MODIFIED Requirements

### Requirement: Mesh 是 RenderGeometry 之上的纯封装

`Mesh` SHALL 继承 `RefObject`，持有 `CounterPtr<RenderGeometry>` + `std::vector<SubMesh>` + `std::vector<BlendShape>`，提供 `SetGeometry` / `GetGeometry` / `AddSubMesh` / `GetSubMeshes` / `AddBlendShape` / `GetBlendShapes` / `GetBlendShapeCount` / `GetName` / `GetLocalBounds`（委托 `geometry->GetLocalBounds()`，返回 `BoundingBoxSphere`）。`Mesh` SHALL NOT 直接持有底层 buffer（buffer 在 `RenderGeometry` 里）。

`SubMesh` SHALL 含 `firstVertex` / `vertexCount` / `firstIndex` / `indexCount` / `materialIndex` / `bounds`（`BoundingBoxSphere`）。`firstVertex`/`vertexCount` 定义子网格顶点区间，`firstIndex`/`indexCount` 定义索引区间。`materialIndex` 是材质/technique 占位索引。

#### Scenario: 挂接 geometry 与 submesh

- **WHEN** `mesh.SetGeometry(geo)` 后 `mesh.AddSubMesh({firstVertex=0, vertexCount=24, firstIndex=0, indexCount=36, materialIndex=0, bounds})`
- **THEN** `GetGeometry() == geo`，`GetSubMeshes().size() == 1` 且字段一致；`GetName()` 返回构造时传入的 Name
