## 1. core/math 图元生成（GeometryGenerator.h）

- [x] 1.1 `GeometryStreams` 结构（positions/normals/tangents/uvs/indices，SoA）
- [x] 1.2 `GenerateCube` / `GeneratePlane`
- [x] 1.3 `GenerateSphere`（UV sphere）
- [x] 1.4 `GenerateCylinder` / `GenerateCone`
- [x] 1.5 `GenerateCapsule`（圆柱 + 两半球）

## 2. aurora 装配（BuiltinGeometry.h）

- [x] 2.1 `BuiltinGeometry::Build`：4 个分离 `VertexBuffer`（POSITION/NORMAL/TANGENT/UV1）+ `IndexBuffer` + `Upload` + 组装 `RenderGeometry` + `SetLocalBounds`
- [x] 2.2 索引 U32 默认 / U16 降级（越界返回 null）

## 3. 测试

- [x] 3.1 `core/test/GeometryGeneratorTest.cpp`：各图元顶点/索引计数、法线单位化、tangent 正交 + w∈{±1}、uv∈[0,1]、索引不越界
- [x] 3.2 `aurora/core/test/BuiltinGeometryTest.cpp`：`Build` 后 4 个分离 stream 语义 + index 类型 + bounds

## 4. 验证与收尾

- [x] 4.1 `cmake --build` Core + AuroraCore 通过
- [x] 4.2 `CoreTest` + `AuroraCoreTest` 全绿
- [ ] 4.3 `openspec archive aurora-builtin-geometry` 归档（需用户确认）
