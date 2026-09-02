## 1. 接口层：AccessFlagBit 收敛

- [x] 1.1 `aurora/rhi/Core.h`：重写 `AccessFlagBit` 为 stage-agnostic 类别（`NONE` / `INDIRECT_BUFFER` / `INDEX_BUFFER` / `VERTEX_BUFFER` / `CBV` / `SRV` / `UAV` / `RTV` / `DSV` / `DSV_READ` / `COPY_SRC` / `COPY_DST` / `PRESENT` / `GENERAL`）
- [x] 1.2 `aurora/rhi/Barrier.h`：删除 `InferStageForAccess` 声明
- [x] 1.3 `aurora/rhi/Barrier.cpp`：删除 `InferStageForAccess` 实现；重写 `InferLayoutForAccess` 映射（`RTV`→`COLOR_ATTACHMENT`、`DSV`→`DEPTH_STENCIL_ATTACHMENT`、`DSV_READ`→`DEPTH_STENCIL_READ_ONLY`、`SRV`→`SHADER_READ_ONLY`、`UAV`→`GENERAL`、`COPY_SRC/DST`→`TRANSFER_SRC/DST`、`PRESENT`→`PRESENT`、多类→`GENERAL`、`NONE`→`UNDEFINED`）

## 2. 后端转换

- [x] 2.1 `vulkan/src/VulkanConversion.cpp`：access 类别 → `VkAccessFlags2`（`SRV`→`SHADER_SAMPLED_READ`、`UAV`→`SHADER_STORAGE_READ|WRITE`、`RTV`→`COLOR_ATTACHMENT_WRITE`、`DSV`→`DEPTH_STENCIL_ATTACHMENT_WRITE`、`DSV_READ`→`DEPTH_STENCIL_ATTACHMENT_READ`、`COPY_*`→`TRANSFER_*`、`CBV`→`UNIFORM_READ`）；stage 沿用 `PipelineStageFlags` 直映射
- [x] 2.2 `dx12/src/D3D12Conversion.cpp`：access 类别 → `D3D12_RESOURCE_STATES`（`SRV`→`PIXEL|NON_PIXEL_SHADER_RESOURCE`、`UAV`→`UNORDERED_ACCESS`、`RTV`→`RENDER_TARGET`、`DSV`/`DSV_READ`→`DEPTH_WRITE`/`DEPTH_READ`、`COPY_*`→`COPY_SOURCE`/`COPY_DEST`）
- [x] 2.3 确认 `metal` / `gles` 不引用被删的 bit（Metal 用粗粒度 memoryBarrier，无逐 bit 映射；无 GLES 后端）

## 3. RDG 适配

- [x] 3.1 `interface/src/rdg/RenderGraph.cpp`：`COLOR_WRITE`→`RTV`、`DEPTH_STENCIL_WRITE`→`DSV`、`TRANSFER_READ/WRITE`→`COPY_SRC`/`COPY_DST`
- [x] 3.2 `interface/src/rdg/Compile.cpp`：`MakeImageBarrier` / `MakeBufferBarrier` 的 src/dst stage 改为由 pass 类型推断（新增 `StageForAccess(access, passIndex)`：raster→`COLOR_OUTPUT`/`EARLY|LATE_FRAGMENT`/`VERTEX|FRAGMENT_SHADER`；compute→`COMPUTE_SHADER`；copy→`TRANSFER`），不再调 `InferStageForAccess`
- [x] 3.3 `interface/include/aurora/rdg/RDGGraph.h` / `RenderGraph.h` / `RenderGraphBuilder.h`：access 常量替换 + `StageForAccess` / 新 barrier helper 签名

## 4. 测试

- [x] 4.1 `test/BarrierInferTest.cpp`：`InferLayoutForAccess` 新映射的断言（`RTV`/`SRV`/`DSV`/`DSV_READ`/`UAV`/`COPY_*`/冲突/空）
- [x] 4.2 `test/BarrierTest.cpp`：barrier 场景改用新 access（`RTV`、`UAV`）
- [x] 4.3 `test/RDGTest.cpp`：access 常量替换（`NONE` 不变，无 `FRAGMENT_SRV` 等旧 bit）
- [x] 4.4 全量编译 + 跑 `AuroraTest`（114 tests 全绿，Vulkan + DX12，validation 无新 warning）

## 5. 文档

- [x] 5.1 `engine/aurora/AGENTS.md`：更新「AccessFlags → ImageLayout」表与「Stage / Access 兼容性」表为 stage-agnostic 类别
- [ ] 5.2 `openspec archive aurora-simplify-access`
