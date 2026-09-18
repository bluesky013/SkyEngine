## MODIFIED Requirements

### Requirement: SceneRaster pass execute 由 DrawItem 驱动

`AddSceneRasterPass` SHALL 只接收 setup（`AddSceneRasterPass(name, setup)`），不再接收 execute lambda。execute 由 `SceneRasterPayload` 数据驱动。

`DrawItem` SHALL 新增 `uint32_t batchDynamicOffset`（默认 0）；executor 绑 set 2 SHALL 使用 `BindResourceGroup(2, item.batchResourceGroup, 1, &item.batchDynamicOffset)`（当 batch RG 非空时）。

executor 在 item 有 `vb` 时 SHALL 先 `BindVertexBuffers(0, 1, &{item.vb, item.vbOffset, 0})` 再 `BindIndexBuffer(item.ib, item.ibOffset, IndexType::U32)` 与 `DrawIndexed(item.args)`；SHALL NOT 跳过顶点缓冲绑定。

#### Scenario: SceneRaster items 驱动绘制
- **WHEN** `SceneRasterPassBuilder.AddDrawItem(item)` 收集 item，`Compile()` 后 `Execute()`
- **THEN** executor 遍历 queues 的 items，逐 item `BindResourceGroup(2, batchRG, 1, &batchDynamicOffset)` / `BindPipeline` / `DrawIndexed`

#### Scenario: 带顶点缓冲的 item 绑定 IA

- **WHEN** item 的 `vb != nullptr`
- **THEN** executor 在 `BindIndexBuffer` / `DrawIndexed` 前调用 `BindVertexBuffers(0, 1, ...)`，传入 `item.vb` 与 `item.vbOffset`

#### Scenario: 无顶点缓冲的 item 不崩

- **WHEN** item 的 `vb == nullptr`
- **THEN** executor 跳过顶点缓冲绑定，其余绑定/绘制流程不变

## ADDED Requirements

### Requirement: CopyBlit pass 可声明显式访问类型

`CopyBlitPassBuilder` SHALL 提供 `Src(RDGTextureHandle, AccessFlags)` 与 `Dst(RDGTextureHandle, AccessFlags)`（`Dst(RDGBufferHandle)` 保持 COPY_DST）。内置 fullscreen blit 渲染路径 SHALL 以 `SRV` 声明 src、以 `RTV` 声明 dst，使 barrier 推导把 src 置 `SHADER_READ_ONLY`、dst 置 `COLOR_ATTACHMENT`；拷贝路径 SHALL 继续用默认 `COPY_SRC` / `COPY_DST`。

#### Scenario: 采样 blit 的 barrier

- **WHEN** 一个 CopyBlit pass 以 `Dst(dst, AccessFlagBit::RTV)` / `Src(src, AccessFlagBit::SRV)` 声明
- **THEN** 编译后该 pass 的 front barriers 把 src 转 `SHADER_READ_ONLY`、dst 转 `COLOR_ATTACHMENT`

#### Scenario: 拷贝 blit 保持默认

- **WHEN** 使用 `Dst(dst)` / `Src(src)` 两参重载
- **THEN** 访问类型仍为 `COPY_DST` / `COPY_SRC`
