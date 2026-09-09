## 1. CompiledGraph.h 重构

- [x] 1.1 `CompiledPassType` 扩展为 `{SCENE_RASTER, FULLSCREEN, COMPUTE, COPYBLIT, PRESENT, CUSTOM}`
- [x] 1.2 定义 `DrawItem`（`pso` / `batchResourceGroup` / `vb` / `ib` / `vbOffset` / `ibOffset` / `DrawIndexedArgs`）
- [x] 1.3 定义 `SceneRasterPayload`（`passResourceGroup` / `colors` / `depthStencil` / `TransientVector<DrawItem> items`）
- [x] 1.4 定义 `FullScreenPayload`（`pso` / `passResourceGroup` / `colors` / `depthStencil`）
- [x] 1.5 定义 `ComputePayload`（`pso` / `passResourceGroup` / `groupX` / `groupY` / `groupZ`）
- [x] 1.6 定义 `CopyBlitPayload`（`kind` / `srcBuffer` / `dstBuffer` / `srcImage` / `dstImage` / `size` / offsets）
- [x] 1.7 定义 `PresentPayload`（`image`）
- [x] 1.8 定义 `CustomPayload`（`std::function<void(RDGContext&, CommandBuffer&)> fn`）
- [x] 1.9 `CompiledPass` 重构：`payload` 改为 `std::variant<SceneRasterPayload, FullScreenPayload, ComputePayload, CopyBlitPayload, PresentPayload, CustomPayload>`，删除三个 `std::function` 字段
- [x] 1.10 `CompiledGraph` 新增 `globalResourceGroup`（set 0，pipeline 全局一个）

## 2. Compile.cpp 适配

- [x] 2.1 `ProduceCompiledGraph` 构造 variant payload：raster pass → `SCENE_RASTER` + `SceneRasterPayload`（colors/depthStencil，items 为空）
- [x] 2.2 compute pass → `COMPUTE` + `ComputePayload`（groups 默认 1,1,1）
- [x] 2.3 copy pass → `COPYBLIT` + `CopyBlitPayload`（kind 默认 BUFFER）

## 3. Execute.cpp 适配

- [x] 3.1 `ExecutePasses` `switch (pass.type)` dispatch（保持旧逻辑，后续 builder change 填充 items 后切换）
- [x] 3.2 `SCENE_RASTER`：`BeginRendering` + 旧 executeFn 逻辑（items 为空时 fallback）
- [x] 3.3 `COMPUTE`：旧 executeFn 逻辑
- [x] 3.4 `COPYBLIT`：旧 executeFn 逻辑

## 4. 验证

- [x] 4.1 编译通过
- [x] 4.2 `AuroraTest --gtest_filter=RDG*` 全绿（6/6）

## 5. 收尾

- [ ] 5.1 `openspec archive aurora-rdg-compiledgraph-variant` 归档本 change
