## Context

`aurora-render-buffers` 已落地 `RenderResource`（buffer/image 公共基类：惰性创建 + 统一 `Upload`）与 buffer 侧具体类型；`aurora-render-geometry` 落地复合网格资源。`RenderResource` 明确声明「buffer 与 image 的公共基类」，image 子类位置已预留但未实现。

RHI 层现有 image 相关接口：

- `rhi::Image`（`Image::Descriptor`：`imageType`/`format`/`extent`/`mipLevels`/`arrayLayers`/`samples`/`usage`/`viewUsage`/`memory`），由 `Device::CreateImage` 创建。
- `ImageType`：`IMAGE_1D`/`IMAGE_2D`/`IMAGE_3D`（cube 与 2D array 都是 `IMAGE_2D`，靠 `arrayLayers` 与 `viewUsage` 区分）。
- `ImageViewType`：`VIEW_2D`/`VIEW_2D_ARRAY`/`VIEW_CUBE`/`VIEW_CUBE_ARRAY`/`VIEW_3D`（当前 RHI 尚无 `Image::View` 类，view 类型暂不落地，等 RHI view 落地后由维度类型消费）。
- `Queue::UploadImage(image, vector<ImageUploadRequest>)`（`aurora-upload` 已落地）：`ImageUploadRequest` 含 `source`/`offset`/`size`/`mipLevel`/`layer`/`bufferRowLength`/`bufferImageHeight`/`imageOffset`/`imageExtent`。
- 工具：`GetImageFormatInfo` / `GetImageRowPitch` / `GetImageSlicePitch`（block-compressed 感知）。
- `sky::Result<T> = std::pair<bool, T>`（`core/template/Result.h`，Core 链接可用）。

**旧 `engine/render` 的既有参考**（`render/core/include/render/resource/Texture.h` / `TextureAtlas.h`，`namespace sky`）：`Texture`（`IStreamableResource`，image + `ImageViewPtr` + `SamplerPtr` 三合一）、`Texture2D`/`TextureCube`/`Texture3D`、以及 atlas 侧 `TextureAtlasAllocator`（抽象 `Allocate(w,h)->Page`）+ `TextureLinearAllocator` + `Texture2DAtlas : public Texture2D`。

本 change 与旧系统的两处**有意差异**：

1. **命名**：aurora 用 `namespace sky::aurora`，与旧 `sky::Texture*` 不冲突（迁移期两者共存，旧代码待移除）。
2. **语义边界**：aurora 的 `Texture` 是**纯 `Image` 封装**（不含 view/sampler）——aurora RHI 尚无 `ImageView` 类，sampler 在 ResourceGroup 绑定层，故「image-only」在 aurora 成立；而 atlas 的**打包器接口对齐旧引擎**（见 D5）。

约束：`namespace sky::aurora`；无新增第三方；复用 `RenderResource` 与 `Queue::UploadImage`，不新增 RHI 接口；header-only（沿用 `Buffer.h` 模式）。

## Goals / Non-Goals

**Goals:**

- `Texture`：image 侧的 `RenderResource` 子类（非模板），惰性 `CreateImage` + `UploadImage` 委托 `Queue::UploadImage`；提供维度访问器。
- 维度特化类型 `Texture2D`/`TextureCube`/`Texture2DArray`/`Texture3D`（继承 `Texture`），各自就地设置维度默认值并提供维度便利 `Init`。
- `TextureAtlas`（继承 `Texture2D`）：打包分配器接口 `TextureAtlasAllocator` + `TextureLinearAllocator`，`Allocate(w,h)` 分配子区域、`Upload(page, data, size)` 上传子区域。
- 最小测试：维度默认值、惰性创建、上传 smoke、atlas 打包/子区域上传。

**Non-Goals:**

- 不做 `RenderTarget` / `StorageTexture` / `TransientTexture`（渲染目标/存储/瞬态 image tier）——这些需要不同 `usage`（`RENDER_TARGET`/`STORAGE`/`TRANSIENT`）与生命周期，属后续 change。
- 不做 `Image::View` / SRV / sampler（RHI 尚无 view 类；sampler 绑定在 ResourceGroup 层）。
- 不做 mip 链自动生成（mip 生成是 RHI/blit 职责）。
- 不改 `aurora-render-buffers` 契约，不改 RHI 接口。

## Decisions

### D1: 位置与文件（header-only）

新代码放 `engine/aurora/core/include/aurora/resource/Texture.h`，纳入 `Aurora`（core）目标 `file(GLOB_RECURSE ...)` 自动收集，不改 `CMakeLists.txt`。测试放 `engine/aurora/core/test/TextureResourceTest.cpp`。

- **理由**：与 `Buffer.h`/`RenderGeometry.h` 同目录同模式（全 header-only，`src/resource/` 现为空）。
- **备选**：新建 `aurora/resource` 独立模块 —— 被否，规模不足，与 buffer 层一致留在 core。

### D2: `Texture` 是非模板基类（image 侧的 RenderResource 子类，纯 image 封装）

纹理层**不引入 Kind tag**。buffer 层的 Kind tag 存在的唯一理由是 `kind × tier` 两个正交维度（`VertexBuffer<DynamicBuffer<VertexBufferKind>>` 表达全组合）；纹理 v1 只有一档 tier（GPU_ONLY），没有正交组合，Kind 只是「把几个常量转给模板」的死重量。故 `Texture` 直接做成非模板基类：

```cpp
class Texture : public RenderResource {
public:
    Texture() = default;
    explicit Texture(const Name &inName) : RenderResource(inName) {}

    // Raw/generic entry: caller supplies the full Image::Descriptor (imageType /
    // viewUsage included). Base only enforces the v1 texture invariant (GPU_ONLY).
    bool Init(Device *dev, const Image::Descriptor &inDesc);

    // Unified upload: full mip-0, layer-0, tightly-packed data (convenience).
    bool Upload(const void *data, uint64_t size, uint64_t offset = 0) override;
    // Sub-resource upload: per-mip/per-layer/per-region.
    bool UploadImage(const std::vector<ImageUploadRequest> &requests);

    Image       *GetImage() const;
    const Image::Descriptor &GetDescriptor() const;
    Extent3D     GetExtent() const;
    uint32_t     GetMipLevels() const;
    uint32_t     GetArrayLayers() const;
    PixelFormat  GetFormat() const;

protected:
    void Create() override;   // device->CreateImage(desc)
    void Release() override;

    Image::Descriptor desc;
    ImagePtr image;
};
```

`Init(dev, inDesc)`：`desc = inDesc`，仅强制 `desc.memory = MemoryType::GPU_ONLY`（v1 纹理不变式）；`imageType`/`viewUsage`/`usage` 由调用方（或维度子类）决定。

`Upload(data, size, offset)` 把 `data` 当作整张 **mip0、layer0 紧密打包** 数据，构造单个 `ImageUploadRequest{source=RawBufferStream(data,size), offset=offset, size=size, mipLevel=0, layer=0, imageExtent=full extent}` 后调 `UploadImage`。`RawBufferStream` 复用 `Buffer.h` 已有的 `IUploadStream` 实现。

- **理由**：`RenderResource` 是「单个 rhi 资源封装」，`Texture` 是其 image 特化；非模板避免了无正交收益的模板间接。维度语义由子类提供（见 D4），未来 `RenderTarget`/`StorageTexture` 是语义不同的**兄弟类**，各自实现 `Create`/上传语义。
- **备选**：`Texture<Kind>` 模板 + Kind tag（镜像 Buffer.h）—— 被否：纹理无 `kind × tier` 正交，Kind 无组合收益，`ImageViewType` 也暂无 RHI 消费方。

### D3: `Upload` 的 `offset` 语义（与 buffer 的差异需显式）

`RenderResource::Upload(data, size, offset)` 的 `offset` 在 buffer 子类是**目的字节偏移**（`BufferUploadRequest::dstOffset`），在 `Texture` 里是**源字节偏移**（`ImageUploadRequest::offset`，image 没有字节级目的偏移，目的子区域用 `imageOffset`/`imageExtent` 表达，走 `UploadImage`）。

- **理由**：同一签名在不同资源类别语义不同是既有基类设计的既定事实（buffer 是「写到哪里」，image 是「从哪读 + 覆盖哪块」，后者更细粒度，完整表达在 `UploadImage`）。此处显式记录，避免调用方误用。
- **备选**：给 `Texture` 另立不带 `offset` 的便捷函数 —— 被否，基类纯虚 `Upload` 必须实现，且「源偏移」与 `ImageUploadRequest` 语义一致。

### D4: 维度默认值由子类就地设置（不经 tag 中转）

不设独立 Kind 结构体，各维度类型的 `Init` 直接构造 `Image::Descriptor` 填好 `imageType`/`viewUsage`/`usage`/`arrayLayers` 默认值后调 `Texture::Init`：

```cpp
class Texture2D : public Texture {
public:
    bool Init(Device *dev, PixelFormat format, Extent2D extent, uint32_t mipLevels = 1, SampleCount samples = SampleCount::X1);
};

class TextureCube : public Texture {
public:
    bool Init(Device *dev, PixelFormat format, Extent2D extent, uint32_t mipLevels = 1);  // arrayLayers forced 6
};

class Texture2DArray : public Texture {
public:
    bool Init(Device *dev, PixelFormat format, Extent2D extent, uint32_t arrayLayers, uint32_t mipLevels = 1);
};

class Texture3D : public Texture {
public:
    bool Init(Device *dev, PixelFormat format, Extent3D extent, uint32_t mipLevels = 1);
};
```

各 `Init` 填好 `format`/`extent`/`mipLevels`/`arrayLayers`（cube 强制 6 + `CUBE_MAP_COMPATIBLE`、2D 强制 1、2DArray 用入参、3D 用 `extent.depth` 且 `arrayLayers=1`），`usage` 默认 `SAMPLED | TRANSFER_DST`，再调基类 `Init(dev, desc)`。`extent.depth` 对 2D/2DArray/cube 恒为 1。

- **理由**：维度默认值是「维度语义」的一部分，就地写在对应子类里最直观，调用方 `TextureCube::Init(dev, fmt, size)` 即可。
- **备选**：全走基类 `Init(dev, desc)` 让调用方自拼 descriptor —— 被否，回到易错起点。

### D5: `TextureAtlas` = 2D 纹理 + allocator 打包器（对齐旧引擎）

旧引擎的「atlas 接口」是 allocator 打包器，本 change 对齐移植：

```cpp
// 打包器接口（移植旧 TextureAtlasAllocator）
class TextureAtlasAllocator {
public:
    TextureAtlasAllocator(uint32_t width, uint32_t height) : width(width), height(height) {}
    virtual ~TextureAtlasAllocator() = default;

    struct Page {
        uint32_t x;
        uint32_t y;
        uint32_t w;
        uint32_t h;
    };

    virtual Result<Page> Allocate(uint32_t w, uint32_t h) = 0;

protected:
    uint32_t width;
    uint32_t height;
};

// left→right, top→bottom, 永不释放的线性打包器
class TextureLinearAllocator : public TextureAtlasAllocator {
public:
    TextureLinearAllocator(uint32_t w, uint32_t h) : TextureAtlasAllocator(w, h) {}
    Result<Page> Allocate(uint32_t w, uint32_t h) override;

private:
    uint32_t currentX  = 0;
    uint32_t currentY  = 0;
    uint32_t rowHeight = 0;
};

// atlas = 2D 纹理 + 一个打包器
class TextureAtlas : public Texture2D {
public:
    using Page = TextureAtlasAllocator::Page;

    bool Init(Device *dev, PixelFormat format, Extent2D extent, uint32_t mipLevels = 1);
    void SetAllocator(std::unique_ptr<TextureAtlasAllocator> allocator);

    Result<Page> Allocate(uint32_t w, uint32_t h);
    bool Upload(const Page &page, const void *data, uint64_t size);

private:
    std::unique_ptr<TextureAtlasAllocator> allocator;
};
```

- `Init` 委托 `Texture2D::Init`，默认安装 `TextureLinearAllocator`（extent 即 atlas 尺寸）；`SetAllocator` 允许注入自定义打包器（shelf/bin-packing 等）。
- `Allocate(w,h)` 转发 allocator，返回 texel 空间 `Page{x,y,w,h}`（`Result` 的 bool 为 false 表示装不下）。
- `Upload(page, data, size)` 构造单个 `ImageUploadRequest{imageOffset={page.x,page.y,0}, imageExtent={page.w,page.h,1}}`（紧密打包）转发 `UploadImage`。
- `std::unique_ptr<TextureAtlasAllocator>` 使 atlas 天然不可拷贝（与 `RenderGeometry` 的 `unique_ptr` 所有权一致）。
- `sky::Result<T>` 来自 `core/template/Result.h`（Core 链接可用），与旧引擎 `Allocate` 返回 `Result<Page>` 一致。

- **理由**：旧 `Texture2DAtlas : public Texture2D` 已经是「纹理 + allocator」的正确形态；打包是 atlas 的核心职责（不打包的「查表」不是 atlas）。命名沿用 `TextureAtlas`（继承 `Texture2D` 已隐含 2D）。
- **备选**：命名区域查表（`AddRegion`/`FindRegion`）—— 被否，review 确认旧引擎 atlas 语义是 allocator 打包器；查表可作未来上层便利，非本 change。

### D6: 上传委托 `Queue::UploadImage`（复用 aurora-upload）

纹理上传不内含 staging/`isUMA` 判断。`Texture::UploadImage` 只把 `ImageUploadRequest` 列表转发给 `Queue::UploadImage`，路径选择（staging vs 直写、in-flight）由 RHI 的 `aurora-upload` 决定（同 buffer 层契约）。`StaticBuffer` 用 `RawBufferStream` 包裹 host 指针，`Texture` 复用同一实现。

- **理由**：resource 层零 staging 代码，拓扑知识留在 RHI。
- **备选**：resource 层自建 image staging —— 被否，与 buffer 层契约矛盾。

### D7: v1 纹理一律 GPU_ONLY（无 tier 正交）

buffer 层有 static/dynamic/transient 三档 tier，但纹理的「更新维度」与 buffer 不同（纹理不持久 map 写入，更新走 staging upload）。v1 纹理只支持 `GPU_ONLY`（上传一次），基类 `Init` 强制 `memory = GPU_ONLY`。`RenderTarget`/`StorageTexture`/`TransientTexture` 属后续 change。

- **理由**：避免过早引入「image tier × 维度」的组合爆炸；贴图是压倒性主用例。
- **备选**：镜像 buffer 三档 tier —— 被否，纹理没有「每帧 CPU 写」的对等物，先做最小面。

## Risks / Trade-offs

- **[`Upload` 只覆盖 mip0/layer0]** 便捷 `Upload` 语义限定为「整张 mip0、layer0 紧密打包」；对 cube（6 面）/ 2DArray（N 层）**只上传第 0 层**，多 layer/mip/子区域走 `UploadImage`。→ 缓解：spec 显式写明 layer 陷阱，cube/2DArray 调用方须用 `UploadImage` 传全部层。
- **[`ImageUploadRequest` 需要调用方算 row/slice pitch]** block-compressed（BC/ASTC）的紧密打包 pitch 由 RHI 的 `GetImageRowPitch`/`GetImageSlicePitch` 提供。→ 缓解：便捷 `Upload`/atlas `Upload` 传 `bufferRowLength=0` 让 RHI 按 tight 处理；需要 stride 的调用方用 `UploadImage` + 工具函数。
- **[命名与旧 `sky::Texture*` 重叠]** aurora `sky::aurora::Texture*` 与旧 `sky::Texture*` 并存。→ 缓解：命名空间隔离；这是迁移期的有意重复，旧 render 代码移除后自然消解。
- **[纯 image 封装，不含 view/sampler]** 与旧 `Texture`（image+view+sampler 三合一）不同。→ 缓解：aurora 尚无 `ImageView`，sampler 在 ResourceGroup 层，image-only 是正确的 aurora 边界；RHI view 落地后由维度类型扩展。
- **[atlas 打包器不可拷贝]** `unique_ptr` 成员删拷贝。→ 缓解：atlas 作为共享资源经 `CounterPtr`/移动传递，符合 `RenderGeometry` 先例。
- **[`TextureLinearAllocator` 永不释放]** 线性打包只增不减，碎片无法回收。→ 缓解：与旧引擎一致（v1 语义）；更高打包密度用 `SetAllocator` 注入 shelf/bin 打包器。

## Migration Plan

1. `Texture.h` 落地（`Texture` 基类 + 维度类型 + allocator 打包器 + `TextureAtlas`）。
2. `TextureResourceTest.cpp`：维度默认值 + 惰性创建 + 上传 smoke + atlas 打包/子区域上传。
3. archive。

## Open Questions

- `RenderTarget`/`StorageTexture`/`TransientTexture` 的形态（何种 `usage` + 何种生命周期）——后续 change，本 change 不涉及。
- `Image::View` 何时落地（RHI 层）——落地后由维度类型消费对应 `ImageViewType`。
- 更多打包器（shelf/bin-packing）——`SetAllocator` 扩展点已预留，后续按需注入。
