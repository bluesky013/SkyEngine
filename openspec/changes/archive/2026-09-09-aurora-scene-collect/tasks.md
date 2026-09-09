## 1. RDG queue technique tag

- [x] 1.1 `CompiledGraph.h`：`SceneRasterQueue` 新增 `Name techniqueTag`（默认空）
- [x] 1.2 `RenderGraph.h/.cpp`：`AddQueue(passIndex, name, sortPolicy, tag)` 三参重载；两参版本委托三参（tag 为空）
- [x] 1.3 `RenderGraphBuilder.h` + 实现：`AddQueue(name, sortPolicy, tag)` 三参重载
- [x] 1.4 `Compile.cpp`：`ProduceCompiledGraph` 拷贝 `techniqueTag` 到 payload

## 2. aurora scene 抽象（aurora/pipeline/scene/）

- [x] 2.1 `SceneView.h`：frustum（6 plane）+ view/viewProject 矩阵 + `FrustumCulling(const AABB&)`；`SetPerspective`/`SetOrthogonal`/`SetViewMatrix`
- [x] 2.2 `RenderPrimitive.h`：`TechniqueBinding{pso, batchResourceGroup}`；geometry（vb/ib/offsets/`CmdDrawIndexed args`）+ `AABB worldBounds` + `std::unordered_map<Name, TechniqueBinding, Name::Hash> techniques` + `GatherRenderItem(ctx)`（tag 命中才 append；空 tag 不过滤）
- [x] 2.3 `RenderScene.h/.cpp`：`AddPrimitive`/`RemovePrimitive`/`GetPrimitives`；views 管理（`AddView`/`GetView`）

## 3. SceneRasterPassTemplate 收集链路

- [x] 3.1 `SetScene(RenderScene*)` / `SetView(SceneView*)` 成员
- [x] 3.2 `Collect(SceneRasterPassBuilder&)` 默认实现：对 builder 内每个 queue（按声明序），用 queue.techniqueTag 过滤 + view.FrustumCulling 剔除 → `AddDrawItem(queue, item)`
- [x] 3.3 收集后按 queue.sortPolicy 排序（FRONT_TO_BACK 深度升序 / BACK_TO_FRONT 降序 / NONE 不排）

## 4. OpaquePass 接入

- [x] 4.1 `OpaquePass`：queue tag "opaque"；`Collect` 走模板默认实现
- [x] 4.2 builder 需要暴露 queue 遍历（Collect 默认实现需访问 pass 内已声明 queues 及其 tag/sortPolicy）

## 5. 测试

- [x] 5.1 mock scene：3 个 primitives（opaque only / shadow only / opaque+shadow），view frustum 覆盖前两个
- [x] 5.2 断言 opaque queue 只收 opaque primitives；shadow tag 互不污染
- [x] 5.3 断言 frustum 外 primitive 被剔除
- [x] 5.4 断言 FRONT_TO_BACK 排序（深度升序）
- [x] 5.5 空 tag queue 收集所有 primitives（不过滤）

## 6. 验证与收尾

- [x] 6.1 全量 `cmake --build` 通过
- [x] 6.2 `AuroraTest` 全绿
- [ ] 6.3 `openspec archive aurora-scene-collect` 归档本 change
