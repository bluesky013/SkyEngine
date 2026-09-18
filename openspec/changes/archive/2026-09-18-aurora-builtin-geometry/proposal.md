## Why

aurora 已有 mesh 接口（`MeshData.h`/`Mesh.h`）与 `RenderGeometry`，但没有任何内置几何体生成能力——要一个 cube/sphere/capsule 得手写顶点/索引数据。引擎开发、demo、调试（灯光/材质/后处理测试）都需要标准图元。

需求：补内置几何体（cube、胶囊体等），**算法放 `core/math`**（纯参数化曲面数学，产出 SoA 数组），**geometry 装配放 aurora**（产出 `RenderGeometry`）；顶点数据 **SoA 分离 stream**，position 独立成流，**基础属性带 uv / normal / tangent**。

## What Changes

- **新增 `core/math/GeometryGenerator.h`**（纯数学，无 aurora/rhi 依赖）：
  - `struct GeometryStreams`：SoA 数组——`positions`(Vector3) / `normals`(Vector3) / `tangents`(Vector4，w=handedness) / `uvs`(Vector2) / `indices`(uint32)。
  - 自由函数：`GenerateCube` / `GeneratePlane` / `GenerateSphere`(UV sphere) / `GenerateCylinder` / `GenerateCone` / `GenerateCapsule`。
  - 每个图元生成 position + normal + tangent（对齐 UV U 方向）+ uv + 三角形索引（CCW）。
- **新增 `aurora/resource/BuiltinGeometry.h`**（aurora 装配层）：
  - `BuiltinGeometry::Build(Device*, const GeometryStreams&, IndexType)` → `CounterPtr<RenderGeometry>`。
  - 建 4 个分离 `VertexBuffer`（`POSITION`/`NORMAL`/`TANGENT`/`UV1` 语义）+ `IndexBuffer`，`Upload`，组装进 `RenderGeometry`，`SetLocalBounds`（由 positions 算 min/max）。
  - 索引 `U32` 默认，可选 `U16`（顶点数 < 65536 时降级）。

## Capabilities

### New Capabilities

- `aurora-builtin-geometry`: `core/math` 参数化图元生成算法（`GeometryStreams` SoA 输出）+ `aurora/resource` 的 `BuiltinGeometry` 装配（分离 stream 的 `RenderGeometry`）。

### Modified Capabilities

（无 —— 复用 `aurora-mesh` 的 `MeshData`/`Mesh` 与 `RenderGeometry`/`VertexBuffer`/`IndexBuffer`，不改其契约。）

## Impact

- **新增文件**：`engine/core/include/core/math/GeometryGenerator.h`（header-only，随 `Core` GLOB）、`engine/aurora/core/include/aurora/resource/BuiltinGeometry.h`（header-only）。
- **测试**：
  - `engine/core/test/GeometryGeneratorTest.cpp`：各图元顶点/索引计数正确、法线单位化、tangent 正交、uv ∈ [0,1]。
  - `engine/aurora/core/test/BuiltinGeometryTest.cpp`：`Build` 后 `RenderGeometry` 有 4 个分离 stream（语义正确）+ index + bounds。
- **依赖**：`core/math`（Vector2/Vector3/Vector4）、`aurora/resource`（`RenderGeometry`/`Buffer`/`MeshData`）。
- **不影响**：RHI 接口层、`Mesh`/`RenderGeometry` 契约。

## Non-Goals

- 不做 LOD / meshlet 化的图元。
- 不做 icosphere / 细分曲面（tessellation）。
- 不做骨骼/蒙皮的 builtin 几何（仅静态图元）。
- 不做索引缓存 / 顶点去重优化（v1 直接按参数化布局输出）。
