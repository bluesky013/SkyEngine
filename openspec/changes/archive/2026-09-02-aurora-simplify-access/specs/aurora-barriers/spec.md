## MODIFIED Requirements

### Requirement: AccessFlags ↔ PipelineStage ↔ Layout 转换语义

`AccessFlags` SHALL 表达 **stage-agnostic 的资源访问类别**，不再编码 shader stage：

- `SRV`（shader 只读，合并所有 `*_SRV`）
- `UAV`（shader 读写 / storage，合并所有 `*_UAV_READ` / `*_UAV_WRITE`）
- `CBV`（constant buffer，合并所有 `*_CBV`）
- `RTV`（render target 写，合并 `COLOR_WRITE` / `COLOR_INOUT_WRITE` / `COLOR_READ` / `COLOR_INPUT`）
- `DSV`（depth/stencil 写，合并 `DEPTH_STENCIL_WRITE` / `DEPTH_STENCIL_INOUT_WRITE`）
- `DSV_READ`（depth/stencil 读，合并 `DEPTH_STENCIL_READ` / `DEPTH_STENCIL_INOUT_READ` / `DEPTH_STENCIL_INPUT`）
- `COPY_SRC`（`TRANSFER_READ`）、`COPY_DST`（`TRANSFER_WRITE`）
- `PRESENT`、`VERTEX_BUFFER`、`INDEX_BUFFER`、`INDIRECT_BUFFER`、`GENERAL`、`NONE`

stage 由调用方在 `BarrierInfo.srcStage` / `dstStage` **显式指定**（RDG 按 pass 类型推断），不再从 access 反解。后端实现 SHALL 把 `AccessFlags`、`PipelineStageFlags`、`ImageLayout` 按以下原则翻译：

- Vulkan：access 类别直接映射 `VkAccessFlags2`（`SRV`→`SHADER_READ`，`UAV`→`SHADER_WRITE`，`RTV`→`COLOR_ATTACHMENT_WRITE`，…）；stage 由 `srcStage`/`dstStage` 直接映射 `VkPipelineStageFlags2`
- DX12：聚合 access 推导 `D3D12_RESOURCE_STATES`（transition before/after）；UAV → UAV barrier；layout 概念 noop
- Metal：access → `MTLBarrierScope` + `MTLRenderStages`；layout 概念 noop
- GLES：access → `glMemoryBarrier` 位；layout / stage 忽略

后端 MUST 在 debug build 下校验 `AccessFlags` 与 `oldLayout`/`newLayout` 的一致性。

#### Scenario: RTV → SRV 转换
- **WHEN** 调用方在 EndRendering 之后插入 image barrier：`srcAccess=RTV, dstAccess=SRV, oldLayout=COLOR_ATTACHMENT, newLayout=SHADER_READ_ONLY, srcStage=COLOR_OUTPUT, dstStage=FRAGMENT_SHADER`
- **THEN** 后续在另一 pass 采样该 image 不报错；GPU readback 验证内容正确

#### Scenario: COPY_DST → RTV 转换
- **WHEN** 调用方在 CopyBufferToImage 之后 BeginRendering 用同一 image 作为 RT，barrier `srcAccess=COPY_DST, dstAccess=RTV, oldLayout=TRANSFER_DST, newLayout=COLOR_ATTACHMENT, srcStage=TRANSFER, dstStage=COLOR_OUTPUT`
- **THEN** 渲染输出包含 transfer 写入的初始内容（LoadOp::LOAD 时）

### Requirement: InferLayoutForAccess 工具函数

接口层 SHALL 提供自由函数 `ImageLayout InferLayoutForAccess(AccessFlags access)`，按以下规则映射（access 为 stage-agnostic 类别）：

- access 仅含 `RTV` → `COLOR_ATTACHMENT`
- access 仅含 `DSV` → `DEPTH_STENCIL_ATTACHMENT`
- access 仅含 `DSV_READ` → `DEPTH_STENCIL_READ_ONLY`
- access 仅含 `SRV` → `SHADER_READ_ONLY`
- access 含 `UAV` → `GENERAL`
- access 仅含 `COPY_SRC` → `TRANSFER_SRC`
- access 仅含 `COPY_DST` → `TRANSFER_DST`
- access 仅含 `PRESENT` → `PRESENT`
- access 含多类冲突 → `GENERAL`
- access 为空 (`NONE`) → `UNDEFINED`

调用方 MAY 用此函数减少手写 layout 的负担：

```cpp
ImageBarrierInfo b{};
b.image = img;
b.srcAccess = RTV;
b.dstAccess = SRV;
b.oldLayout = InferLayoutForAccess(b.srcAccess);
b.newLayout = InferLayoutForAccess(b.dstAccess);
```

#### Scenario: 单 access 推导
- **WHEN** 调用 `InferLayoutForAccess(RTV)`
- **THEN** 返回 `COLOR_ATTACHMENT`

#### Scenario: SRV 推导
- **WHEN** 调用 `InferLayoutForAccess(SRV)`
- **THEN** 返回 `SHADER_READ_ONLY`

#### Scenario: 冲突 access 退化到 GENERAL
- **WHEN** 调用 `InferLayoutForAccess(RTV | SRV)`
- **THEN** 返回 `GENERAL`（写 + 读收敛不到单一 layout）

#### Scenario: 空 access
- **WHEN** 调用 `InferLayoutForAccess(AccessFlagBit::NONE)`
- **THEN** 返回 `UNDEFINED`
