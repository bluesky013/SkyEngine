## 1. CPU 侧数据接口（MeshData.h）

- [x] 1.1 `MeshVertexDataInterface`：抽象 `Resize/GetStride/Count/GetData`（含 const 重载）+ 模板 `SetVertexData`/`GetVertexData`
- [x] 1.2 `TRawMeshVertexData<T>`：具体实现（`BinaryDataPtr`，stride = sizeof(T)）
- [x] 1.3 `RawMeshIndexData`：U16/U32 索引数据（`Resize/Count/GetData/SetIndex/GetIndex/GetIndexType`）

## 2. 资源侧 Mesh（Mesh.h）

- [x] 2.1 `SubMesh` 结构（firstVertex/vertexCount/firstIndex/indexCount/materialIndex/bounds）
- [x] 2.2 `BlendShape` 结构（name + positionDeltas/normalDeltas/tangentDeltas）
- [x] 2.3 `Skeleton` 资源（`Bone{name, parent, inverseBind}` + 骨骼列表）
- [x] 2.4 `Mesh : RefObject`：`SetGeometry`/`GetGeometry`/`GetLocalBounds`/`AddSubMesh`/`GetSubMeshes`/`AddBlendShape`/`GetBlendShapes`/`GetBlendShapeCount`/`SetSkeleton`/`GetSkeleton`/`HasSkin`/`GetName`

## 3. 顶点语义扩展（VertexSemantic.h）

- [x] 3.1 `VertexSemantic` 加 `JOINTS`/`WEIGHTS`，`kVertexSemanticCount` 13→15，更新 `VertexSemanticName`/`ParseVertexSemantic`
- [x] 3.2 迁移 `ShaderVariantTest` 的 `HAS_SKIN` 顶点变体语义 `CUSTOM1/CUSTOM2` → `JOINTS/WEIGHTS`（及任何 shader asset 的 `@vertex` 块）

## 4. 测试

- [x] 4.1 `core/test/MeshResourceTest.cpp`：数据接口构建/存取（顶点 + U16/U32 索引）
- [x] 4.2 `Mesh` 结构：submesh 字段 + blendshape delta 注册/读取 + skeleton 挂接
- [x] 4.3 `RenderGeometry` 挂接：`SetGeometry` 后 `GetGeometry` 返回同一对象
- [x] 4.4 顶点语义：`JOINTS`/`WEIGHTS` 可 `ParseVertexSemantic` 往返

## 5. 验证与收尾

- [x] 5.1 `cmake --build` AuroraCore 通过
- [x] 5.2 `AuroraCoreTest` 全绿
- [ ] 5.3 `openspec archive aurora-mesh` 归档（需用户确认）
