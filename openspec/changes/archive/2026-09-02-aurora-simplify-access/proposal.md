## Why

`AccessFlagBit` 当前是 D3D12 味儿的细粒度模型——39 个 bit 把 `stage × read/write × resource` 全部糅进一个枚举（`VERTEX_SRV` / `FRAGMENT_UAV_WRITE` / `COMPUTE_CBV` / …）。调用方要用「任意 shader 阶段读一张贴图」得写 `VERTEX_SRV | FRAGMENT_SRV | COMPUTE_SRV`，barrier 推导也因此被迫从 access 里反解 stage。这在 Vulkan 1.3 / Metal 上既不必要又啰嗦，是时候把 access 收敛成 stage-agnostic 的资源访问类别，让 stage 单独表达。

## What Changes

- **BREAKING**：重写 `AccessFlagBit` 为 stage-agnostic 访问类别，从 39 个 bit 收敛到 ~15 个：
  - `SRV`（shader 只读，合并所有 `*_SRV`）
  - `UAV`（shader 读写 / storage，合并所有 `*_UAV_READ` / `*_UAV_WRITE`）
  - `CBV`（constant buffer，合并所有 `*_CBV`）
  - `RTV`（render target 写，合并 `COLOR_WRITE` / `COLOR_INOUT_WRITE`）、`RTV_READ`（color input 读）
  - `DSV`（depth/stencil 写）、`DSV_READ`（depth/stencil 读）
  - `COPY_SRC`（`TRANSFER_READ`）、`COPY_DST`（`TRANSFER_WRITE`）
  - `PRESENT`、`VERTEX_BUFFER` / `INDEX_BUFFER` / `INDIRECT_BUFFER`（顶点输入，保留）、`GENERAL`、`NONE`
- **stage 与 access 解耦**：`PipelineStageFlags` 已经是 `BarrierInfo` 的独立字段；`InferStageForAccess(access)` 改名为 / 改为「access 类别 + stage 提示」的推断（RDG 按 pass 类型给 stage；直接 RHI 用户显式指定）。
- `InferLayoutForAccess` 重写为新 access 类别的 layout 映射（`SRV`→`SHADER_READ_ONLY`，`UAV`→`GENERAL`，`RTV`→`COLOR_ATTACHMENT`，…）。
- 后端转换重写：`VulkanConversion`（access→`VkAccessFlags2`）、`D3D12Conversion`（access→D3D12 states）；Metal/GLES 本来就用粗粒度 memoryBarrier，不受影响。
- RDG builder 与内部 barrier 推导改用新 access（`COLOR_WRITE`→`RTV`，`TRANSFER_*`→`COPY_*`，`FRAGMENT_SRV`→`SRV`）。
- 更新 `aurora-barriers` 相关测试与 AGENTS.md 的 access→layout / access→stage 表。

## Capabilities

### New Capabilities

（无）

### Modified Capabilities

- `aurora-barriers`: AccessFlags 的 stage-agnostic 收敛、`InferLayoutForAccess` 的新映射、以及 stage 与 access 的解耦语义。

## Impact

- **接口层**：`Core.h`（`AccessFlagBit` 枚举）、`Barrier.h/.cpp`（`InferLayoutForAccess` / `InferStageForAccess`）
- **后端**：`VulkanConversion.{h,cpp}`、`D3D12Conversion.{h,cpp}`（Metal 的 layout/stage 是 noop，仅需确认不引用被删的 bit）
- **RDG**：`RenderGraph.cpp` / `Compile.cpp` / `RDGGraph.h` / `RenderGraphBuilder.h`（access 常量替换）
- **测试**：`BarrierTest.cpp`、`BarrierInferTest.cpp`、`RDGTest.cpp`
- **文档**：`engine/aurora/AGENTS.md` 的「AccessFlags → ImageLayout」与「Stage / Access 兼容性」两表
