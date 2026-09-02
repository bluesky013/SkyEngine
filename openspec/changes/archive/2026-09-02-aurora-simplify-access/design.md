## Context

`AccessFlagBit`（`aurora/rhi/Core.h`）当前是 D3D12 味儿的细粒度模型：39 个 bit 把 `shader stage × read/write × resource` 糅进一个枚举（`VERTEX_SRV` / `FRAGMENT_UAV_WRITE` / `COMPUTE_CBV` / …）。

问题：

1. 调用方表达「任意 shader 阶段读一张贴图」要写 `VERTEX_SRV | FRAGMENT_SRV | COMPUTE_SRV`。
2. `InferStageForAccess(access)`（aurora-rdg 引入）被迫从 access 里反解 stage——stage 本不该藏在这里。`BarrierInfo` 已经有独立的 `srcStage` / `dstStage` 字段，是 stage 的正确归宿。
3. Vulkan 1.3 的 `VK_ACCESS_2_*` 本身就是 stage-agnostic（`SHADER_READ_BIT` 不区分阶段），Metal 的 `MTLBarrierScope` / `MTLRenderStages` 也是分开的两轴。当前模型是反其道而行。

本 change 把 access 收敛成 **stage-agnostic 的资源访问类别**，让 stage 回到 `BarrierInfo.srcStage/dstStage`。

## Goals / Non-Goals

**Goals:**
- `AccessFlagBit` 从 39 bit 收敛到 ~13 bit 的访问类别。
- stage 与 access 解耦：stage 由 `BarrierInfo.srcStage/dstStage` 显式表达。
- 重写 `InferLayoutForAccess`、移除 `InferStageForAccess`（stage 改由 RDG 按 pass 类型推断）。
- 后端转换（Vulkan / DX12）改按新类别映射；RDG 常量替换；测试更新。

**Non-Goals:**
- 不改 `PipelineStageFlags`（已经是独立枚举）。
- 不引入 subpass input-attachment 专属 access（aurora 用 dynamic rendering，无 subpass）。
- 不改 Metal / GLES 的 barrier 语义（本来就用粗粒度 memoryBarrier，layout/stage 是 noop）。
- 不做 barrier 合并/elide 优化（属于各后端 `PipelineBarrier` 实现，独立 change）。

## Decisions

### 决策 1：新 AccessFlagBit 枚举

```cpp
enum class AccessFlagBit : uint64_t {
    NONE            = 0x00000000,
    INDIRECT_BUFFER = 0x00000001,
    INDEX_BUFFER    = 0x00000002,
    VERTEX_BUFFER   = 0x00000004,
    CBV             = 0x00000008,
    SRV             = 0x00000010,
    UAV             = 0x00000020,
    RTV             = 0x00000040,
    DSV             = 0x00000080,
    DSV_READ        = 0x00000100,
    COPY_SRC        = 0x00000200,
    COPY_DST        = 0x00000400,
    PRESENT         = 0x00000800,
    GENERAL         = 0x00001000,
};
```

合并关系（旧 → 新）：
- `*_SRV` → `SRV`
- `*_UAV_READ` / `*_UAV_WRITE` → `UAV`
- `*_CBV` → `CBV`
- `COLOR_WRITE` / `COLOR_INOUT_WRITE` / `COLOR_READ` / `COLOR_INOUT_READ` / `COLOR_INPUT` → `RTV`
- `DEPTH_STENCIL_WRITE` / `DEPTH_STENCIL_INOUT_WRITE` → `DSV`
- `DEPTH_STENCIL_READ` / `DEPTH_STENCIL_INOUT_READ` / `DEPTH_STENCIL_INPUT` → `DSV_READ`
- `TRANSFER_READ` → `COPY_SRC`；`TRANSFER_WRITE` → `COPY_DST`
- `SHADING_RATE` → 删除（无使用）
- `COLOR_READ` / `COLOR_INPUT` 并入 `RTV`（dynamic rendering 无 subpass input，读 color attachment 的场景罕见）

**Why:** 类别化后调用方只写 `SRV` / `UAV`，barrier 语义更贴近 Vulkan 1.3 / Metal 的两轴模型。

### 决策 2：stage 解耦

- `InferStageForAccess(access)` **移除**（access 不再含 stage，反解无意义）。
- stage 的来源：
  - **RDG**：`DeriveBarriers` 按 access 记录的 pass 类型 + access 类别推断 src/dst stage（raster→`COLOR_OUTPUT`/`EARLY|LATE_FRAGMENT`/`VERTEX|FRAGMENT_SHADER`；compute→`COMPUTE_SHADER`；copy→`TRANSFER`）。
  - **直接 RHI 用户**：显式填 `BarrierInfo.srcStage/dstStage`（本来就是显式的）。
- `MakeImageBarrier`/`MakeBufferBarrier` 的 stage 由调用方传入（不再内部调 `InferStageForAccess`）。

**Why:** stage 属于 barrier 的同步轴，本就该独立；由 pass 类型推断比从 access 反解更准确（`SRV` 在 raster pass 是 fragment/vertex，在 compute pass 是 compute）。

### 决策 3：InferLayoutForAccess 新映射

- `RTV` → `COLOR_ATTACHMENT`
- `DSV` → `DEPTH_STENCIL_ATTACHMENT`
- `DSV_READ` → `DEPTH_STENCIL_READ_ONLY`
- `SRV` → `SHADER_READ_ONLY`
- `UAV`（单独或与其它混合）→ `GENERAL`
- `COPY_SRC` → `TRANSFER_SRC`；`COPY_DST` → `TRANSFER_DST`
- `PRESENT` → `PRESENT`
- 多类冲突 → `GENERAL`；`NONE` → `UNDEFINED`

**Why:** 与旧表一一对应，只是 key 换成类别。

### 决策 4：后端转换

- **Vulkan**（`VulkanConversion`）：`SRV`→`VK_ACCESS_2_SHADER_READ_BIT`、`UAV`→`VK_ACCESS_2_SHADER_STORAGE_READ_BIT|WRITE`、`RTV`→`COLOR_ATTACHMENT_WRITE`、`DSV`→`DEPTH_STENCIL_ATTACHMENT_WRITE`、`DSV_READ`→`DEPTH_STENCIL_ATTACHMENT_READ`、`COPY_*`→`TRANSFER_*`、`CBV`→`UNIFORM_READ`。stage 用 `PipelineStageFlags` 直映射（已有）。
- **DX12**（`D3D12Conversion`）：`SRV`→`D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|NON_PIXEL_SHADER_RESOURCE`、`UAV`→`UNORDERED_ACCESS`、`RTV`→`RENDER_TARGET`、`DSV`/`DSV_READ`→`DEPTH_WRITE`/`DEPTH_READ`、`COPY_*`→`COPY_SOURCE`/`COPY_DEST`。
- **Metal / GLES**：无变化（粗粒度，不引用被删 bit 即可）。

### 决策 5：RDG 常量替换

- `SetColorAttachment`：`COLOR_WRITE` → `RTV`
- `SetDepthStencilAttachment`：`DEPTH_STENCIL_WRITE` → `DSV`
- `SetCopySrc/Dst`：`TRANSFER_READ/WRITE` → `COPY_SRC`/`COPY_DST`
- 用户侧 `b.Read(handle, FRAGMENT_SRV)` → `b.Read(handle, SRV)`；stage 由 RDG 按 pass 类型给。

## Risks / Trade-offs

- **BREAKING API**：所有调用方（含测试、未来 renderer）都要换新常量。一次性改完，后续无迁移成本。
- **stage 粒度变粗**：`SRV` 不再区分 vertex/fragment 阶段。RDG 按 pass 类型给 `VERTEX|FRAGMENT`（raster）或 `COMPUTE`，比旧模型略保守但正确；确需更细阶段时由调用方显式指定 stage。
- **DX12 promotion/decay**：粗粒度 access 可能让 DX12 后端少做一些 barrier 合并（它依赖 state 精确）。v1 用 `NON_PIXEL|PIXEL_SHADER_RESOURCE` 这类宽 state 覆盖，后续再在 DX12 后端内做 promotion/decay 优化。
- **UAV 读写合一**：`UAV` 不再分 READ/WRITE，hazard 分析（RAR/RAW/WAR/WAW）需要额外信息时从「是否有写」语义判断；对 storage 资源默认按读写处理。

## Migration Plan

1. 改 `Core.h` 枚举 + `Barrier.cpp`（`InferLayoutForAccess` + 删 `InferStageForAccess`）。
2. 改 `VulkanConversion` / `D3D12Conversion`。
3. 改 RDG（`RenderGraph.cpp` / `Compile.cpp` / `RDGGraph.h` / `RenderGraphBuilder.h`）的 access 常量 + stage 推断。
4. 改测试（`BarrierTest` / `BarrierInferTest` / `RDGTest`）。
5. 更新 `AGENTS.md` 的 access→layout / access→stage 两表。
6. 全量编译 + 跑 Vulkan 测试。
