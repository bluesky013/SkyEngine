## Why

`aurora-scene-collect` 落地的 `RenderScene` 是指针式结构：`std::vector<RenderPrimitive*>` + 每 primitive 一个 `std::unordered_map<Name, TechniqueBinding>`。收集时每帧 `N × (指针解引用 + 哈希查找)`，内存不连续、cache 不友好，且后续场景组件（light / skin / camera 等）叠加后指针模型会进一步劣化。framework 虽有 `EntityManager` 但仍是 stub（`AddComponent` 返回 nullptr），不可用。

将 RenderScene 改为**纯 ECS data-oriented**：core 层提供通用 `SparseSet` 容器（基础容器，后续引擎各处可复用），aurora scene 基于它做 entity 注册 + SoA 组件数组。

## What Changes

### Part 1 — core ECS 基础容器（`core/ecs/`）

- **新增** `EntityId`（`uint32_t`：24 bit index + 8 bit generation，支持 id 复用后旧引用失效检测）。
- **新增** `SparseSet<T>`：sparse 页表（`sparse[id] → dense index`）+ dense 数组（entity 与 component 双数组对齐）；`Add` / `Remove`（swap-remove 保 dense）/ `Get` / `Contains` / 连续迭代；顺序扫描无指针跳转。
- **新增** `EntityRegistry`：entity 创建/销毁（free list + generation）+ 多 `SparseSet` 组件池管理（按 type id 索引）。

### Part 2 — scene 迁入 `aurora/core` 并 ECS 化

- **迁移** `engine/aurora/pipeline/{include/aurora/pipeline/scene, src/scene}` → `engine/aurora/core/{include/aurora/scene, src/scene}`（scene 抽象归属 aurora core 顶层 renderer 模块；`aurora/pipeline` 链接 `Aurora` target 引用）。
- **重构** `RenderScene`：内嵌 `EntityRegistry`；`AddPrimitive/RemovePrimitive` 改为 `CreateEntity/DestroyEntity` + 组件挂载；views 保留独立 registry。
- **新增** 场景组件（`aurora/scene/SceneTypes.h`，SoA 存储）：
  - `Bounds`（AABB worldBounds）
  - `RenderItem`（过渡形态：`{ Name techniqueTag; DrawItem item; }`——pso/batchRG/vb/ib/args 全在 item 内；techniqueTag 保住现有 queue 过滤语义；**未来 technique 设计落地后此组件被替换**）
  - `Light`（type / color / direction / intensity，占位）
  - `Skin`（占位）
- **删除** `RenderPrimitive` 独立结构（`GatherRenderItem` 逻辑移入 Collect；`GatherContext` / `TechniqueBinding` map 一并删除）。
- **适配** `SceneRasterPassTemplate::Collect`：以 `Bounds` 池 dense 数组为主驱动遍历，按 entity 查 `RenderItem` 并做 tag 过滤；排序语义不变。

## Capabilities

### New Capabilities

- `core-ecs`: `EntityId` / `SparseSet<T>` / `EntityRegistry` 的契约（id 稳定性、generation 失效检测、dense 迭代、swap-remove）。

### Modified Capabilities

- `aurora-scene-collect`: `RenderScene` 改为 ECS 存储（entity + SoA 组件）；`RenderPrimitive` 删除拆解为组件；Collect 遍历 dense 数组。

## Impact

- **core**：新增 `core/ecs/EntityId.h` / `core/ecs/SparseSet.h` / `core/ecs/EntityRegistry.h` + 测试。
- **aurora core**：scene 目录迁入（SceneView / RenderScene / SceneTypes.h）；`RenderScene.h/.cpp` 重构；`RenderPrimitive.h` 删除。
- **aurora pipeline**：`SceneRasterPassTemplate.cpp` Collect 适配；CMake 链接 `Aurora` target。
- **测试**：core ECS 容器测试；SceneCollectTest 适配（mock 改 `CreateEntity + Add<T>`，断言行为不变）。
- **不影响**：queue techniqueTag 过滤语义、排序、RDG 层。
