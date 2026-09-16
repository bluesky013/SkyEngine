## Why

`aurora-render-buffers` 已落地 `RenderResource` 基类 + 三档 buffer tier + `VertexBuffer`/`IndexBuffer`/`UniformBuffer`/`StorageBuffer` 具体类型；`aurora-render-geometry` 落地了复合网格资源。但 image 侧至今没有渲染层封装——调用方要一张贴图，必须自己拼 `rhi::Image::Descriptor`（`imageType`/`format`/`extent`/`mipLevels`/`arrayLayers`/`samples`/`usage`/`viewUsage`/`memory`）、手写 mip/layer 的 `ImageUploadRequest`，并且没有任何「2D / cube / 2D array / 3D」的维度语义与正确的默认值（cube 要 `arrayLayers=6` + `CUBE_MAP_COMPATIBLE`，2D array 要用户显式给 layer 数，这些都容易出错）。

`RenderResource` 基类在 `aurora-render-buffers` 里已经声明「buffer 与 image 的公共基类」，image 子类位置早已预留；现在补上纹理侧的具体类型，渲染层才能把「裸 `rhi::Image`」升级为「带维度语义、惰性创建、统一上传」的纹理资源。

## What Changes

- 新增 **`Texture` 基类**（非模板，image 侧的 `RenderResource` 子类，`aurora/resource/Texture.h`，header-only）：
  - 持有 `rhi::Image::Descriptor` + `ImagePtr`，惰性 `Create()` 调 `Device::CreateImage`。
  - `Upload(data, size, offset)` 作为「整张 mip0 紧密打包数据」的便捷上传；`UploadImage(std::vector<ImageUploadRequest>)` 作为 per-mip/per-layer/per-region 的完整上传入口，均委托 `Queue::UploadImage`。
  - 访问器 `GetImage()` / `GetDescriptor()` / `GetExtent()` / `GetMipLevels()` / `GetArrayLayers()` / `GetFormat()`。
- 新增**维度特化类型**（继承 `Texture`，各自在 `Init` 里就地设置维度默认值，提供维度便利 `Init`）：
  - **`Texture2D`**（`IMAGE_2D`，`arrayLayers=1`，`VIEW_2D`）
  - **`TextureCube`**（`IMAGE_2D`，强制 `arrayLayers=6`，`CUBE_MAP_COMPATIBLE`，`VIEW_CUBE`）
  - **`Texture2DArray`**（`IMAGE_2D`，用户给定 `arrayLayers=N`，`VIEW_2D_ARRAY`）
  - **`Texture3D`**（`IMAGE_3D`，`depth`，`VIEW_3D`）
- 新增 **`TextureAtlas`**（对齐旧 `engine/render` 的 `Texture2DAtlas`）：继承 `Texture2D` + 打包分配器 `TextureAtlasAllocator`（抽象 `Allocate(w,h) -> Page{x,y,w,h}`）+ `TextureLinearAllocator`（left→right/top→bottom 线性打包），`Allocate` 分配子区域、`Upload(page, data, size)` 上传子区域。
- 复用既有 `rhi::Image` / `Device::CreateImage` / `Queue::UploadImage`（由 `aurora-upload` 能力提供），不新增 RHI 接口。

## Capabilities

### New Capabilities

- `aurora-render-textures`: 纹理侧资源抽象——`Texture` 基类（image 封装、惰性创建、统一上传）+ 维度特化类型 `Texture2D`/`TextureCube`/`Texture2DArray`/`Texture3D` + 图集 `TextureAtlas`（打包分配器 `TextureAtlasAllocator`/`TextureLinearAllocator` + 子区域分配/上传）。

### Modified Capabilities

（无 —— 复用 `aurora-render-buffers` 的 `RenderResource` 基类与 `aurora-upload` 的 `Queue::UploadImage` 上传能力，不新增 buffer 语义、不改 RHI 接口。）

## Impact

- **新文件**：`engine/aurora/core/include/aurora/resource/Texture.h`（header-only，随 `aurora/core` GLOB 自动纳入）。
- **测试**：`engine/aurora/core/test/` 新增 `TextureResourceTest.cpp`（维度默认值 + 惰性创建 + 上传 smoke + atlas 打包/子区域上传）。
- **依赖**：`aurora/rhi`（`Image`/`Device::CreateImage`/`Queue::UploadImage`/`ImageUploadRequest`/`ImageType`/`ImageViewType`/`ImageUsageFlagBit`/`ImageViewUsageFlagBit`）、`core`（`RefObject`、`Name`）、`aurora-upload`（`UploadImage` 上传能力，已落地）。
- **不影响**：`aurora-render-buffers` 既有 buffer 类型与测试；RHI/RDG 接口层；`RenderGeometry`。
