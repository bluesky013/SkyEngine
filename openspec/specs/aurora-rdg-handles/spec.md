# aurora-rdg-handles Specification

## Purpose
TBD - created by archiving change aurora-rdg. Update Purpose after archive.
## Requirements
### Requirement: RDGTextureHandle / RDGBufferHandle 强类型 opaque ID

Aurora RDG SHALL 提供两个独立的 handle 类型：

```cpp
struct RDGTextureHandle { uint32_t id = INVALID_INDEX; };
struct RDGBufferHandle  { uint32_t id = INVALID_INDEX; };
```

不允许隐式转换。Handle 仅在所属 RenderGraph 实例的生命周期内有效；跨 graph / 跨帧使用 handle 是未定义行为。

`IsValid()` 方法返回 `id != INVALID_INDEX`。

#### Scenario: 类型不匹配编译期拒绝
- **WHEN** 调用方 `b.Read(textureHandle, ...)` 但 builder 的 Read 签名期待 `RDGBufferHandle`
- **THEN** 编译错误（强类型保护）

#### Scenario: 跨 graph handle 使用被检测
- **WHEN** Debug build 下，从 graph A 拿到的 handle 被传给 graph B 的 Read
- **THEN** assert 触发（handle 内部带 owner-graph epoch tag 用于校验）

### Requirement: Handle 生命周期分析

Compile 期 SHALL 对每个 handle 计算 `(firstUsePass, lastUsePass)` 区间。该区间用于：
- Transient pool alias 决策
- 推导 export 资源（lastUsePass 是终点的 import-原点资源）的最终 transition
- Pass culling（lastUsePass 决定何时"释放"transient slot）

Read pass / Write pass / Attachment pass 都计入使用。

#### Scenario: 单一 use pass
- **WHEN** Texture T 仅被 Pass A 写
- **THEN** firstUsePass = lastUsePass = A.index；transient pool 在 A 结束后立即可回收 T

#### Scenario: 跨多 pass 使用
- **WHEN** Texture T 被 Pass A 写、Pass C 读、Pass D 读
- **THEN** firstUsePass = A.index；lastUsePass = D.index；T 在 [A, D] 区间内被池占用

### Requirement: RDGContext 提供运行时资源访问

execute lambda 收到 `RDGContext &ctx` 参数。`ctx` SHALL 提供：

- `GetTexture(RDGTextureHandle) -> rhi::Image*` — 返回当前 pass 中该 handle 实际绑定的 RHI Image
- `GetBuffer(RDGBufferHandle) -> rhi::Buffer*` — 同上
- `GetPassName() -> const char*` — 调试用
- `GetCommandBuffer() -> rhi::CommandBuffer*` — 与 Execute 传入的同一对象

execute lambda **不得**调用 `GetCommandBuffer()->PipelineBarrier`（barrier 由 RDG 提供）；MAY 调用 cmdBuf 上的非 barrier 接口（如 push constants 等需要 cmdbuf-级访问的工具，理论上）。

#### Scenario: execute lambda 取得当前 RHI 资源
- **WHEN** Compute pass 内 `auto *buf = ctx.GetBuffer(handle); auto *encoder = ...; encoder->BindResourceGroup(0, group);`
- **THEN** buf 是本帧 transient 池实际分配/复用的 RHI Buffer*；group 中的 buffer 写入对应该 buf

### Requirement: Handle equality 与 hash

RDG handle SHALL 支持 `operator==` / `operator!=`；MAY 提供 `std::hash<RDGTextureHandle>` 特化以便用作 unordered_map key（用于 pass 内的 ResourceGroup 缓存等场景）。

#### Scenario: handle 作为 map key
- **WHEN** 调用方 `std::unordered_map<RDGTextureHandle, CounterPtr<ResourceGroup>> cache;`
- **THEN** 编译通过；cache 操作正常

