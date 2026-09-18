## Why

aurora 的资源层已有 `VertexBuffer`（带 `VertexLayout`）、`IndexBuffer`（带 `IndexType`）、`RenderGeometry`（vertex streams + index + AABB 的纯封装），但缺少「mesh 接口」这一层：

1. **无 CPU 侧数据构建接口**：调用方要建网格数据，只能直接操作 `std::vector` 再喂给 `Upload(data, size)`，没有「类型无关的顶点数据接口 + 索引数据接口」来统一构建（旧 `engine/render` 有 `MeshInterface.h`：`MeshVertexDataInterface`/`TRawMeshVertexData<T>`/`RawMeshIndexData`）。
2. **无高层 `Mesh` 资源**：`RenderGeometry` 只是「几何」，没有 submesh（多材质分段）、blend shape（morph target）、bounds 语义的完整 mesh 抽象（旧 `engine/render` 有 `Mesh.h`）。

调研了 Unity / Unreal / glTF 2.0 / Filament 的实现，共性结论：mesh 接口 = **顶点数据布局（分离 or interleaved）** + **submesh/primitive 分段** + **blend shape = delta 增量混合** + 蒙皮/LOD。本 change 落地「CPU 数据接口 + 资源侧 `Mesh` + blend shape 数据结构」，对齐旧引擎与业界模式。

## What Changes

- **新增 CPU 侧数据接口**（`aurora/resource/MeshData.h`，header-only）：
  - `MeshVertexDataInterface`（抽象：`Resize/GetStride/Count/GetData`（含 const 重载）+ 模板 `SetVertexData/GetVertexData`）。
  - `TRawMeshVertexData<T>`（具体，`stride = sizeof(T)`，`core/archive/BinaryData` 承载）。
  - `RawMeshIndexData`（索引数据，U16/U32，按 `IndexType` 存取，`BinaryData` 承载）。
- **新增资源侧 `Mesh`**（`aurora/resource/Mesh.h`，header-only）：
  - `SubMesh` 结构（`firstVertex/vertexCount/firstIndex/indexCount/materialIndex/bounds`），对齐旧 `MeshSubSection`（去掉 meshlet 字段）。
  - `BlendShape` 结构（delta 增量：`name` + `positionDeltas/normalDeltas/tangentDeltas`，与 base 顶点一一对应），对齐 Unity `AddBlendShapeFrame` / glTF morph target。
  - `Mesh : RefObject`，持有 `CounterPtr<RenderGeometry>` + `std::vector<SubMesh>` + `std::vector<BlendShape>` + 可选 `CounterPtr<Skeleton>`，提供 `SetGeometry/AddSubMesh/AddBlendShape/SetSkeleton/HasSkin/GetName` + 访问器。
- **蒙皮**：
  - 顶点语义扩展（`aurora/rhi/VertexSemantic.h`）：加 `JOINTS`（4 骨骼索引）+ `WEIGHTS`（4 权重），对齐 glTF `JOINTS_0`/`WEIGHTS_0`。
  - `Skeleton` 资源（`Bone{name, parent, inverseBind}` + 骨骼列表）。
  - 蒙皮顶点数据（joints/weights）是带 `JOINTS`/`WEIGHTS` 语义的 vertex stream，落在 `RenderGeometry`；`Mesh` 用可选 `CounterPtr<Skeleton>` + `HasSkin()` 表达。
- **material 用 `uint32_t materialIndex` 占位**（材质/technique 系统未开，后续 change 落地）。
- **blend shape / 蒙皮只落数据结构**（delta 增量 / 语义+骨架引用），GPU morphing / GPU 蒙皮执行留后续 change。

## Capabilities

### New Capabilities

- `aurora-mesh`: CPU 侧 mesh 数据接口（`MeshVertexDataInterface`/`TRawMeshVertexData<T>`/`RawMeshIndexData`）+ 资源侧 `Mesh`（`RenderGeometry` + `SubMesh` 分段 + `BlendShape` delta 增量 + `Skeleton` 蒙皮）。

### Modified Capabilities

- `aurora-rhi-core`: `VertexSemantic` 增 `JOINTS`/`WEIGHTS`（`kVertexSemanticCount` 13→15）。

## Impact

- **新增文件**：`engine/aurora/core/include/aurora/resource/MeshData.h`、`engine/aurora/core/include/aurora/resource/Mesh.h`（header-only，随 `aurora/core` GLOB 自动纳入）。
- **修改文件**：`engine/aurora/rhi/interface/include/aurora/rhi/VertexSemantic.h`（加 `JOINTS`/`WEIGHTS`）。
- **测试**：`engine/aurora/core/test/MeshResourceTest.cpp`（数据接口构建/存取 + `Mesh` submesh/blendshape/skeleton 结构 + `RenderGeometry` 挂接）。
- **依赖**：`aurora/resource/Buffer.h`（`IndexType`/`VertexBuffer`）、`aurora/resource/RenderGeometry.h`、`core`（`Name`/`RefObject`/`Vector3`/`AABB`/`BinaryData`/`Matrix4`）。
- **不影响**：`RenderGeometry`、`Buffer`/`Texture` 契约；RHI 接口层（`VertexSemantic` 仅加枚举值，不影响既有 13 语义）。
