## MODIFIED Requirements

### Requirement: RDGContext 提供运行时资源访问

execute lambda 收到 `RDGContext &ctx` 参数。`ctx` SHALL 提供：

- `GetTexture(RDGTextureHandle) -> Image*` — 返回当前 pass 中该 handle 实际绑定的 RHI Image
- `GetBuffer(RDGBufferHandle) -> Buffer*` — 同上
- `GetPassName() -> std::string_view` — 调试用，由 `Name::GetStr()` 提供
- `GetCommandBuffer() -> CommandBuffer*` — 与 Execute 传入的同一对象

execute lambda **不得**调用 `GetCommandBuffer()->PipelineBarrier`（barrier 由 RDG 提供）；MAY 调用 cmdBuf 上的非 barrier 接口（如 push constants 等需要 cmdbuf-级访问的工具，理论上）。

#### Scenario: execute lambda 取得当前 RHI 资源
- **WHEN** Compute pass 内 `auto *buf = ctx.GetBuffer(handle); auto *encoder = ...; encoder->BindResourceGroup(0, group);`
- **THEN** buf 是本帧 transient 池实际分配/复用的 RHI Buffer*；group 中的 buffer 写入对应该 buf

#### Scenario: execute lambda 取得 pass 名称
- **WHEN** Raster pass 内 `std::string_view n = ctx.GetPassName();`
- **THEN** n 为本 pass 注册时传入的 `Name` 对应的字符串内容（interning 存储稳定，view 生命周期安全）
