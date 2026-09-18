# aurora-mesh Specification

## Purpose
TBD - created by archiving change aurora-mesh. Update Purpose after archive.
## Requirements
### Requirement: CPU 侧 mesh 数据接口

`MeshVertexDataInterface` SHALL 是类型无关的顶点数据抽象，提供 `Resize(uint32_t)` / `GetStride()` / `Count()` / `GetData()`（含 const 重载），以及模板 `SetVertexData<T>(index, value, offset)` / `GetVertexData<T>(index, offset)`。`TRawMeshVertexData<T>` SHALL 是其具体实现，`GetStride() == sizeof(T)`，数据用 `core/archive/BinaryData`（`BinaryDataPtr`）承载。

`RawMeshIndexData` SHALL 按 `IndexType`（U16/U32）存取值，提供 `Resize` / `Count` / `GetData` / `SetIndex` / `GetIndex` / `GetIndexType`，数据用 `BinaryData` 承载。

#### Scenario: 顶点数据构建与存取

- **WHEN** 用 `TRawMeshVertexData<Vertex>` 建 N 个顶点，`Resize(N)` 后逐顶点 `SetVertexData`
- **THEN** `Count() == N`、`GetStride() == sizeof(Vertex)`；`GetVertexData(i)` 读回与写入一致

#### Scenario: 索引数据 U16/U32

- **WHEN** 用 `RawMeshIndexData(type=U32)` `SetIndex(i, v)` 后 `GetIndex(i)`
- **THEN** 读回 `v`；`GetData()` 返回可喂 `IndexBuffer::Upload` 的字节指针，`GetIndexType() == U32`

### Requirement: Mesh 是 RenderGeometry 之上的纯封装

`Mesh` SHALL 继承 `RefObject`，持有 `CounterPtr<RenderGeometry>` + `std::vector<SubMesh>` + `std::vector<BlendShape>`，提供 `SetGeometry` / `GetGeometry` / `AddSubMesh` / `GetSubMeshes` / `AddBlendShape` / `GetBlendShapes` / `GetBlendShapeCount` / `GetName` / `GetLocalBounds`（委托 `geometry->GetLocalBounds()`）。`Mesh` SHALL NOT 直接持有底层 buffer（buffer 在 `RenderGeometry` 里）。

`SubMesh` SHALL 含 `firstVertex` / `vertexCount` / `firstIndex` / `indexCount` / `materialIndex` / `bounds`（AABB）。`firstVertex`/`vertexCount` 定义子网格顶点区间，`firstIndex`/`indexCount` 定义索引区间。`materialIndex` 是材质/technique 占位索引。

#### Scenario: 挂接 geometry 与 submesh

- **WHEN** `mesh.SetGeometry(geo)` 后 `mesh.AddSubMesh({firstVertex=0, vertexCount=24, firstIndex=0, indexCount=36, materialIndex=0, bounds})`
- **THEN** `GetGeometry() == geo`，`GetSubMeshes().size() == 1` 且字段一致；`GetName()` 返回构造时传入的 Name

### Requirement: BlendShape 是 delta 增量数据

`BlendShape` SHALL 含 `name` + `positionDeltas`（`std::vector<Vector3>`）+ 可选 `normalDeltas` / `tangentDeltas`（`std::vector<Vector3>`），与 base 顶点一一对应。语义 SHALL 为 `final = base + Σ weightᵢ·deltaᵢ`（对齐 Unity / glTF morph target）。

本能力 SHALL 只落地 delta 数据结构；GPU morphing（morph buffer + shader 混合）SHALL NOT 在本能力内实现。

#### Scenario: 注册 blend shape

- **WHEN** `mesh.AddBlendShape({name, positionDeltas=N 个, normalDeltas=N 个, tangentDeltas 空})`
- **THEN** `GetBlendShapeCount() == 1`；`GetBlendShapes()[0].positionDeltas.size() == N`

#### Scenario: GPU 混合不在本能力

- **WHEN** 阅读 `aurora/resource/Mesh.h`
- **THEN** 无 morph buffer、无 shader 混合代码（blend shape 仅数据结构）

### Requirement: 蒙皮（Skeleton + JOINTS/WEIGHTS 语义）

`VertexSemantic` SHALL 新增 `JOINTS`（4 骨骼索引）与 `WEIGHTS`（4 权重）两个语义，`kVertexSemanticCount` SHALL 从 13 变为 15；蒙皮顶点数据 SHALL 是带这两个语义的 vertex stream（落在 `RenderGeometry`），不引入新类型。

`Skeleton` SHALL 继承 `RefObject`，持有骨骼列表（`Bone{name, parent, inverseBind}`，`parent` 为父骨骼索引，-1 表 root），提供 `GetBones`/`GetBoneCount`。

`Mesh` SHALL 提供 `SetSkeleton(CounterPtr<Skeleton>)` / `GetSkeleton()` / `HasSkin()`（`skeleton != nullptr`）。骨架被多个 Mesh 共享（`CounterPtr`）。

本能力 SHALL 只落「语义 + 骨架 + 引用」；GPU 蒙皮执行（skinning buffer + vertex shader）SHALL NOT 在本能力内实现。

#### Scenario: 蒙皮顶点语义

- **WHEN** 阅读 `VertexSemantic.h`
- **THEN** 含 `JOINTS` / `WEIGHTS` 枚举，`kVertexSemanticCount == 15`；`VertexSemanticName`/`ParseVertexSemantic` 支持两者

#### Scenario: 挂接骨架

- **WHEN** `mesh.SetSkeleton(skel)` 后
- **THEN** `mesh.HasSkin() == true`、`mesh.GetSkeleton() == skel`

#### Scenario: GPU 蒙皮不在本能力

- **WHEN** 阅读 `aurora/resource/Mesh.h`
- **THEN** 无 skinning buffer、无 shader 蒙皮代码（蒙皮仅语义 + 骨架引用）

