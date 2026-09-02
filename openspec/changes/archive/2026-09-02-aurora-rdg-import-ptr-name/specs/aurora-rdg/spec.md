## MODIFIED Requirements

### Requirement: Resource declaration: Import vs Create

`RenderGraph` SHALL 提供：

- `Import(name, ImagePtr) -> RDGTextureHandle` — 引用外部已存在的 image（如 SwapChain backbuffer），graph 持有 `ImagePtr` 引用
- `Import(name, BufferPtr) -> RDGBufferHandle` — 同上，持有 `BufferPtr` 引用
- `CreateTexture(name, RDGTextureDesc) -> RDGTextureHandle` — transient texture，由 RDG 池化
- `CreateBuffer(name, RDGBufferDesc) -> RDGBufferHandle`

其中 `name` 的类型为 `core::Name`；`ImagePtr` / `BufferPtr` 为 `CounterPtr<Image>` / `CounterPtr<Buffer>` 别名。

Import 资源由 graph 持有的智能指针引用保证存活，graph 生命周期内不被释放（graph 销毁时 `CounterPtr` 析构释放引用）；RDG 不会 delete 调用方创建的资源。Transient 资源在 graph 销毁时归还池。

#### Scenario: Import 外部 SwapChain image
- **WHEN** `auto bb = graph->Import(Name("backbuffer"), imagePtr)`，其中 `imagePtr` 是 `CounterPtr<Image>`
- **THEN** 返回有效 handle；后续 pass 可对 bb 做 ColorAttachment / Read；`imagePtr` 超出调用方作用域后底层 image 仍存活（graph 持有引用）

#### Scenario: CreateTexture 命中 transient pool
- **WHEN** 跨帧多次创建相同 desc 的 texture
- **THEN** transient pool 报告 hit；底层 RHI Image 复用（调试可观测 hit/miss 计数）

## ADDED Requirements

### Requirement: Resource 与 pass 名称使用 core::Name

RDG 的资源名与 pass 名 SHALL 使用 `core::Name`（`sky::Name`）而非 `std::string`。所有 `name` 参数与内部存储（`ResourceNode::name`、`PassNode::name`）均为 `Name` 类型。

`Name` 提供 interning 语义：相同字符串对应相同 handle，`operator==` 为 O(1) 比较，`std::hash<Name>` 可用作 unordered 容器 key。

#### Scenario: 相同名称得到相同 Name 身份
- **WHEN** 两个 pass 用相同字符串字面量构造 `Name("post-process")`
- **THEN** 两个 `Name` 相等（`==` 成立），可作为一致的身份标识
