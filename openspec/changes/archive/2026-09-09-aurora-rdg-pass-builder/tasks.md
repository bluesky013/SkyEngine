## 1. RDGGraph.h 重构

- [x] 1.1 `SceneRasterPassData`（`colors` / `depthStencil` / `TransientVector<DrawItem> items`）
- [x] 1.2 `FullScreenPassData`（`pso` / `passResourceGroup` / `colors` / `depthStencil`）
- [x] 1.3 `ComputePassData`（`pso` / `passResourceGroup` / `groups`）
- [x] 1.4 `CopyBlitPassData`（`kind` / `src` / `dst` / `size` / offsets）
- [x] 1.5 `PresentPassData`（`image`）
- [x] 1.6 `CustomPassData`（`std::function`）
- [x] 1.7 `PassTag` 扩展为 `{SceneRasterPassTag, FullScreenPassTag, ComputePassTag, CopyBlitPassTag, PresentPassTag, CustomPassTag}`

## 2. RenderGraphBuilder.h 重构

- [x] 2.1 `SceneRasterPassBuilder`（`ColorAttachment` / `DepthStencilAttachment` / `AddDrawItem`）
- [x] 2.2 `FullScreenPassBuilder`（`SetTechnique` / `SetPassResourceGroup` / `SetTarget` / `SetDepthStencil` / `SetInputSRV`）
- [x] 2.3 `ComputePassBuilder`（`SetPipeline` / `SetPassResourceGroup` / `SetGroups` / `Read` / `Write`）
- [x] 2.4 `CopyBlitPassBuilder`（`Src` / `Dst` / `SetKind` / `SetSize` / `SetOffsets`）
- [x] 2.5 `PresentPassBuilder`（`SetSource`）
- [x] 2.6 `CustomPassBuilder`（`SetCallback` → Read/Write 声明依赖）

## 3. RenderGraph.h / .cpp 重构

- [x] 3.1 `AddSceneRasterPass(name, setup, exec)` — 替代 `AddRasterPass`
- [x] 3.2 `AddFullScreenPass(name, setup)`
- [x] 3.3 `AddCopyBlitPass(name, setup)`
- [x] 3.4 `AddPresentPass(name, setup)`
- [x] 3.5 `AddCustomPass(name, setup, exec)`
- [x] 3.6 `RenderGraph` 内部 `mSceneRasterPasses` / `mFullScreenPasses` / `mCopyBlitPasses` / `mPresentPasses` / `mCustomPasses` 替代 `mRasterPasses` / `mCopyPasses`

## 4. Compile.cpp 适配

- [x] 4.1 `ProduceCompiledGraph` 填充 `SceneRasterPayload.items`（从 `SceneRasterPassData.items` 拷贝）
- [x] 4.2 culling 修正：PRESENT access 的 pass 标记为 live 种子（graph output）

## 5. Execute.cpp 适配

- [x] 5.1 `ExecutePasses` 按 pass tag dispatch 到各 payload 的 emit 逻辑
- [x] 5.2 `SCENE_RASTER`：`BeginRendering` → for item: `BindResourceGroup(2, batchRG)`/`BindPipeline`/`BindIB`/`DrawIndexed` → `EndRendering`
- [x] 5.3 `FULLSCREEN`：`BindResourceGroup(1, passRG)` → `BindPipeline(pso)` → `Draw(3,0,1,0)`
- [x] 5.4 `COMPUTE`：`BindPipeline(pso)` → `BindResourceGroup(1, passRG)` → executeFn
- [x] 5.5 `COPYBLIT`：executeFn（v1 保留 lambda，encoder copy API 后续）
- [x] 5.6 `PRESENT`：无额外 encoder 操作（barrier 已在 frontBarriers）
- [x] 5.7 `CUSTOM`：`fn(ctx, cmdBuf)`

## 6. RDGContext.h 重构

- [x] 6.1 `GetResourceGroup() -> ResourceGroup*` — 返回当前 pass 绑定的持久 ResourceGroup
- [x] 6.2 `SetResourceGroup(ResourceGroup*)` — executor 内部 wiring

## 7. 测试

- [x] 7.1 `RDGTest.cpp`：`AddRasterPass` 改为 `AddSceneRasterPass`；新增 `PassStructureVariant` 测试（6 种 pass 全覆盖）
- [x] 7.2 断言 `CompiledPass.payload` variant 类型正确
- [x] 7.3 断言 `SceneRasterPayload.items` 顺序与 `AddDrawItem` 调用顺序一致

## 8. 验证与收尾

- [x] 8.1 全量 `cmake --build` 通过
- [x] 8.2 `AuroraTest --gtest_filter=RDG*` 全绿（7/7）；AuroraTest 全集 115/115；CoreTest 240/240
- [ ] 8.3 `openspec archive aurora-rdg-pass-builder` 归档本 change
