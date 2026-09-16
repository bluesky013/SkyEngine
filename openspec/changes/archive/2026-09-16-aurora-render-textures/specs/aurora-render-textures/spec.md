## ADDED Requirements

### Requirement: Texture 是 image 侧的 RenderResource 子类（惰性创建 + 统一上传）

`Texture` SHALL 继承 `RenderResource`（非模板），作为 image 资源的封装：持有 `rhi::Image::Descriptor` 与 `ImagePtr`，惰性 `Create()` 调 `Device::CreateImage(desc)`，`IsCreated()` SHALL 反映是否已创建。

`Texture` SHALL 提供统一上传：
- `Upload(data, size, offset)` SHALL 把 `data` 当作整张 **mip0、layer0 紧密打包** 数据上传（`offset` 为**源字节偏移**），构造单个 `ImageUploadRequest{mipLevel=0, layer=0, imageExtent=full extent}` 并转发 `UploadImage`。
- `UploadImage(std::vector<ImageUploadRequest>)` SHALL 委托 `Queue::UploadImage`，SHALL NOT 内含 staging / `isUMA` / in-flight 逻辑（见 `aurora-upload` spec）。

`Texture` SHALL 提供访问器 `GetImage()` / `GetDescriptor()` / `GetExtent()` / `GetMipLevels()` / `GetArrayLayers()` / `GetFormat()`。

#### Scenario: 惰性创建

- **WHEN** 构造一个 `Texture2D` 但尚未访问其底层 image
- **THEN** `IsCreated() == false`、`GetImage() == nullptr`；首次 `Upload`/`UploadImage` 后 `IsCreated() == true`、`GetImage()` 非空

#### Scenario: 上传委托 RHI

- **WHEN** 阅读 `Texture` 的上传实现
- **THEN** 上传经 `Queue::UploadImage` 委托 RHI；`Texture.h` 不含 staging buffer 分配、`isUMA` 判断与 in-flight 判定

#### Scenario: 便捷上传只覆盖 mip0、layer0

- **WHEN** 对一个 `Texture2D` 调 `Upload(data, size)`
- **THEN** 生成的 `ImageUploadRequest` 的 `mipLevel == 0`、`layer == 0`、`imageExtent` 等于整张纹理 extent

#### Scenario: 多 layer 纹理不走便捷上传

- **WHEN** 需要上传 `TextureCube`（6 面）或 `Texture2DArray`（N 层）的全部 layer
- **THEN** 调用方 SHALL 用 `UploadImage` 传每个 layer 的请求（便捷 `Upload` 只覆盖 layer0）

### Requirement: 维度特化类型的默认值

`Texture2D`/`TextureCube`/`Texture2DArray`/`Texture3D` SHALL 继承 `Texture`，各自提供维度便利 `Init(Device*, PixelFormat, extent, ...)`，并 SHALL 设置正确的维度默认值：

- `Texture2D`：`imageType == IMAGE_2D`，`arrayLayers == 1`；
- `TextureCube`：`imageType == IMAGE_2D`，强制 `arrayLayers == 6` 且 `viewUsage` 含 `CUBE_MAP_COMPATIBLE`；
- `Texture2DArray`：`imageType == IMAGE_2D`，`arrayLayers == 入参`；
- `Texture3D`：`imageType == IMAGE_3D`，`arrayLayers == 1`，`extent.depth` 为 3D 深度。

各维度类型默认 `usage` SHALL 含 `SAMPLED | TRANSFER_DST`。

#### Scenario: cube 默认 6 layer + cube-compatible

- **WHEN** `TextureCube::Init(dev, format, {w,h})` 后读 `GetDescriptor()` 与 `GetArrayLayers()`
- **THEN** `GetArrayLayers() == 6`，`desc.viewUsage` 含 `CUBE_MAP_COMPATIBLE`

#### Scenario: 2D array 用户给定 layer 数

- **WHEN** `Texture2DArray::Init(dev, format, {w,h}, 8)` 后读 `GetArrayLayers()`
- **THEN** `GetArrayLayers() == 8`

#### Scenario: 2D 单层

- **WHEN** `Texture2D::Init(dev, format, {w,h})` 后读 `GetArrayLayers()`
- **THEN** `GetArrayLayers() == 1`

### Requirement: v1 纹理一律 GPU_ONLY

`Texture::Init` SHALL 强制 `desc.memory = MemoryType::GPU_ONLY`。v1 SHALL NOT 提供纹理的 dynamic/transient tier；`RenderTarget`/`StorageTexture`/`TransientTexture` SHALL NOT 在本能力内实现。

#### Scenario: 纹理内存为 GPU_ONLY

- **WHEN** 任一维度 `Texture` 完成 `Init` 后读 `GetDescriptor()`
- **THEN** `desc.memory == MemoryType::GPU_ONLY`

### Requirement: TextureAtlas 是 2D 纹理上的 allocator 打包器

`TextureAtlas` SHALL 继承 `Texture2D`，持有一个打包分配器 `std::unique_ptr<TextureAtlasAllocator>`，并提供：

- `Allocate(uint32_t w, uint32_t h)` → `Result<Page>`：委托 allocator 分配一个 texel 空间子区域 `Page{x,y,w,h}`（装不下时 `Result` 的 bool 为 false）；
- `Upload(const Page&, const void* data, uint64_t size)`：把 `data`（紧密打包）上传到 `page` 对应子区域（`imageOffset={page.x,page.y,0}`、`imageExtent={page.w,page.h,1}`）；
- `SetAllocator(std::unique_ptr<TextureAtlasAllocator>)`：注入自定义打包器。

`TextureAtlasAllocator` SHALL 是抽象打包器接口，`TextureLinearAllocator` SHALL 是其 left→right/top→bottom 线性打包实现（永不释放）。`Init` SHALL 默认安装 `TextureLinearAllocator`。

#### Scenario: 分配子区域并上传

- **WHEN** `TextureAtlas::Init(dev, format, {64,64})` 后 `Allocate(16,16)` 得到 `Page p`，再 `Upload(p, data, size)`
- **THEN** `p` 位于 atlas 内（`p.x+p.w <= 64` 且 `p.y+p.h <= 64`）；上传经 `UploadImage` 到 `{p.x,p.y}` 处、extent `{16,16}`

#### Scenario: 连续分配不重叠且不越界

- **WHEN** 依次 `Allocate(32,32)`、`Allocate(32,32)`、`Allocate(32,32)`
- **THEN** 前两个成功且不重叠，第三个（超出 64 高度）返回 `Result` 的 bool 为 false

#### Scenario: 可注入自定义打包器

- **WHEN** 调 `SetAllocator(std::make_unique<TextureLinearAllocator>(w,h))`
- **THEN** 后续 `Allocate` 由注入的 allocator 处理
