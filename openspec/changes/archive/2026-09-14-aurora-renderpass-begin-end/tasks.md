## 1. ScenePass (HDR)

- [x] 1.1 `include/aurora/pipeline/ScenePass.h` + `src/ScenePass.cpp`：继承 `SceneRasterPassTemplate`；`SetExtent(w,h)` / `GetHDRColorHandle()` / `GetDepthHandle()`
- [x] 1.2 `BuildRDG`：`CreateTexture` HDR color（`RGBA16_SFLOAT`，`RENDER_TARGET | TRANSFER_SRC`）+ depth（`D32`，`DEPTH_STENCIL`）；`AddSceneRasterPass` 声明 `ColorAttachment(0, hdr, CLEAR, STORE)` + `DepthStencilAttachment(depth, CLEAR, DONT_CARE, DONT_CARE, DONT_CARE)`；`DeclareQueue("opaque", FRONT_TO_BACK)` + `Collect`；`MarkOfInterest(hdr)`
- [x] 1.3 `OnSetup` / `OnSceneChanged` 占位（PSO 创建等 shader 管线接入后补，与 `OpaquePass` 一致）

## 2. TextureToScreenPass

- [x] 2.1 `include/aurora/pipeline/TextureToScreenPass.h` + `src/TextureToScreenPass.cpp`：继承 `PipelinePass`；`SetInput(RDGTextureHandle)` / `SetOutput(RDGTextureHandle)`
- [x] 2.2 `BuildRDG`：`AddFullScreenPass` 声明 `SetTarget(mOutput, DONT_CARE, STORE)` + `SetInputSRV(mInput)` + `SetPassResourceGroup(...)`（若存在）+ `SetTechnique(GetPSO())`
- [x] 2.3 `OnSetup` / `OnSceneChanged` 占位（fullscreen PSO + pass RG set 1 采样输入 texture，随 shader 管线接入后补）

## 3. 测试

- [x] 3.1 `AuroraPipelineTest` 新增 `ScenePass` BuildRDG smoke test：断言 compiled graph 存在 `SCENE_RASTER` pass，color attachment 为 HDR（loadOp CLEAR）、depth loadOp CLEAR、queues 含 `opaque`（FRONT_TO_BACK）
- [x] 3.2 `AuroraPipelineTest` 新增 `TextureToScreenPass` BuildRDG smoke test：断言 compiled graph 存在 `FULLSCREEN` pass，color attachment loadOp `DONT_CARE`，且输入 texture 与 pass 存在 SRV read 依赖

## 4. 验证与收尾

- [x] 4.1 全量 `cmake --build` 通过
- [x] 4.2 `AuroraPipelineTest` 全绿
- [ ] 4.3 `openspec archive aurora-renderpass-begin-end` 归档本 change
