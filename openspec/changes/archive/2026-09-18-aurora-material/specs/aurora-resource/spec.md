## MODIFIED Requirements

### Requirement: RenderResource 是 buffer/image 的公共资源封装基类

`RenderResource` SHALL 继承 `RefObject`，作为 buffer 与 image 资源的统一非模板基类，作用是对 rhi 资源的一次封装，提供：

- **引用计数**：SHALL 继承 `RefObject`，使 `CounterPtr<Texture>` / `CounterPtr<VertexBuffer>` 等可共享资源；`unique_ptr` 拥有语义（如 `RenderGeometry` 持有 `VertexBuffer`）SHALL 保持可用。
- **惰性创建**：构造时仅持有 `rhi::Device*` 与（`SKY_ENABLE_RESOURCE_NAME == 1` 时的）`Name`，底层 `rhi::Buffer`/`rhi::Image` SHALL 在首次访问（`Create()`）时才创建；`IsCreated()` SHALL 反映是否已创建。
- **统一上传**：`Upload(const void* data, uint64_t size, uint64_t offset = 0)` SHALL 把 host 数据上传到 device 资源，上传策略（staging vs 直接写）SHALL 委托 RHI（见 `aurora-upload` spec），resource 层 SHALL NOT 内含 staging 逻辑。

`RenderResource` SHALL NOT 引入或持有 `rhi::BufferView`，也 SHALL NOT 持有 `offset/range` 视图字段。`name_` SHALL 仅在 `SKY_ENABLE_RESOURCE_NAME == 1` 时存在（见 `aurora-resource-name` spec），`Create()` 时 SHALL 把名字下传 `Descriptor::name`。

#### Scenario: 惰性创建

- **WHEN** 构造一个 buffer 资源但尚未访问其底层 buffer
- **THEN** `IsCreated() == false`，底层 `rhi::Buffer` 尚未创建；首次 `Upload`/访问后 `IsCreated() == true`

#### Scenario: 统一上传入口

- **WHEN** 对一个 buffer 资源调用 `Upload(data, size, offset)`
- **THEN** host 数据被上传到 device 资源指定偏移处，且该资源完成惰性创建

#### Scenario: 引用计数共享

- **WHEN** 两个 `CounterPtr<Texture>` 指向同一 `Texture`，其中一份释放
- **THEN** `Texture` 仍存活；最后一个引用释放时 `Texture` 析构（析构前同步 pending 上传，见 `aurora-upload`）
