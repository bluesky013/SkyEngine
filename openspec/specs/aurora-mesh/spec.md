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

`Mesh` SHALL 继承 `RefObject`，持有 `CounterPtr<RenderGeometry>` + `std::vector<SubMesh>` + `std::vector<BlendShape>`，提供 `SetGeometry` / `GetGeometry` / `AddSubMesh` / `GetSubMeshes` / `AddBlendShape` / `GetBlendShapes` / `GetBlendShapeCount` / `GetName` / `GetLocalBounds`（委托 `geometry->GetLocalBounds()`，返回 `BoundingBoxSphere`）。`Mesh` SHALL NOT 直接持有底层 buffer（buffer 在 `RenderGeometry` 里）。

`SubMesh` SHALL 含 `firstVertex` / `vertexCount` / `firstIndex` / `indexCount` / `materialIndex` / `bounds`（`BoundingBoxSphere`）。`firstVertex`/`vertexCount` 定义子网格顶点区间，`firstIndex`/`indexCount` 定义索引区间。`materialIndex` 是材质/technique 占位索引。

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

### Requirement: 蒙皮输入槽与独立实例绑定
VertexSemantic SHALL 保留 JOINTS / WEIGHTS 及名称解析。JOINTS SHALL 表示四个 Skin 输入槽索引，WEIGHTS SHALL 表示对应权重，数据留在 RenderGeometry 顶点流。Mesh SHALL 不定义或引用 Aurora Skeleton，不持有选定 Skin，不暴露 SetSkeleton/GetSkeleton/HasSkin 或 SetSkin/GetSkin。Skin 与实例绑定、palette 和 GPU 执行 SHALL 遵守 aurora-skinning capability；本类型不直接管理 GPU 蒙皮任务。

#### Scenario: 蒙皮顶点语义
- **WHEN** 读取含 JOINTS / WEIGHTS 的顶点布局
- **THEN** 语义名称 SHALL 能解析，JOINTS SHALL 按绑定槽解释而非直接索引 Skeleton

#### Scenario: 共享 mesh 的实例绑定隔离
- **WHEN** 两个实例对同一 Mesh 选择不同 Skin
- **THEN** 几何资源 SHALL 保持不变，所选绑定 SHALL 由各实例保存
