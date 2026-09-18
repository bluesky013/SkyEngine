## ADDED Requirements

### Requirement: 参数化图元生成（core/math）

`GeometryGenerator`（`core/math`）SHALL 提供 `GenerateCube` / `GeneratePlane` / `GenerateSphere` / `GenerateCylinder` / `GenerateCone` / `GenerateCapsule`，各产出 `GeometryStreams`（SoA）：

- `positions`(Vector3) / `normals`(Vector3) / `tangents`(Vector4，xyz 对齐 UV U 方向，w=±1) / `uvs`(Vector2) / `indices`(uint32)。
- 数组长度一致（顶点属性同长度）；索引为三角形列表（CCW）。

生成算法 SHALL NOT 依赖 aurora/rhi（纯 `core/math`）。

#### Scenario: 图元数据合法

- **WHEN** `GenerateSphere(1.0, 16, 32)` 后检查输出
- **THEN** `positions/normals/tangents/uvs` 长度一致（`(rings+1)*(sectors+1)`）；法线单位化、`dot(tangent.xyz, normal) ≈ 0`、`tangent.w ∈ {±1}`、`uv ∈ [0,1]`；`indices` 为 3 的倍数且索引不越界

### Requirement: BuiltinGeometry 装配为 RenderGeometry（aurora）

`BuiltinGeometry::Build(Device*, const sky::GeometryStreams&, IndexType)` SHALL 产出 `CounterPtr<RenderGeometry>`：

- 4 个分离 `VertexBuffer`，语义分别为 `POSITION` / `NORMAL` / `TANGENT` / `UV1`（stride = 对应类型 sizeof），已 `Upload`。
- 1 个 `IndexBuffer`（`IndexType` 匹配），已 `Upload`。
- `RenderGeometry` 的 `GetLocalBounds()` 由 positions 的 min/max 计算。
- `indexType == U16` 且 `vertexCount ≥ 65536` 时返回 null。

#### Scenario: Build 后 stream 语义与 bounds

- **WHEN** `Build(device, GenerateCube(1.0), IndexType::U32)` 后检查返回的 `RenderGeometry`
- **THEN** `GetVertexStreams().size() == 4`，语义依次含 POSITION/NORMAL/TANGENT/UV1；`GetIndexBuffer()->GetIndexType() == U32`；`GetLocalBounds()` 覆盖所有顶点

#### Scenario: U16 越界返回 null

- **WHEN** 一个 `vertexCount ≥ 65536` 的 `GeometryStreams` 用 `IndexType::U16` 调 `Build`
- **THEN** 返回 `CounterPtr<RenderGeometry>(nullptr)`
