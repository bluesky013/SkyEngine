## 1. Texture 基类落地

- [x] 1.1 `include/aurora/resource/Texture.h`：`class Texture : public RenderResource`（header-only，非模板）
- [x] 1.2 `Init(Device*, Image::Descriptor)`（强制 `memory = GPU_ONLY`）+ 惰性 `Create()`/`Release()`（`Device::CreateImage`）
- [x] 1.3 上传：`Upload(data,size,offset)`（mip0 全图便捷）→ 构造 `ImageUploadRequest` 转发 `UploadImage`；`UploadImage(vector<ImageUploadRequest>)` → `Queue::UploadImage`
- [x] 1.4 访问器：`GetImage()`/`GetDescriptor()`/`GetExtent()`/`GetMipLevels()`/`GetArrayLayers()`/`GetFormat()`

## 2. 维度特化类型 + Atlas

- [x] 2.1 `Texture2D` / `TextureCube`（强制 `arrayLayers=6` + `CUBE_MAP_COMPATIBLE`）/ `Texture2DArray` / `Texture3D`：继承 `Texture`，维度便利 `Init(Device*, PixelFormat, extent, ...)` 就地填默认值
- [x] 2.2 `TextureAtlasAllocator`（抽象 `Allocate(w,h)->Result<Page>`）+ `TextureLinearAllocator`（left→right/top→bottom 线性打包）
- [x] 2.3 `TextureAtlas : public Texture2D`：`Init`（默认装 `TextureLinearAllocator`）/`SetAllocator`/`Allocate`/`Upload(page,data,size)`（子区域 `ImageUploadRequest`）

## 3. 测试

- [x] 3.1 `core/test/TextureResourceTest.cpp`：维度默认值（2D 单层、cube=6 layer + cube-compatible、2DArray=N、3D depth）
- [x] 3.2 `TextureLazyCreateAndUpload`（Vulkan）：惰性创建 + `Upload`/`UploadImage` smoke + 访问器
- [x] 3.3 `TextureAtlasPack`：`Allocate` 连续分配不重叠/不越界（第三个返回 false）+ `Upload(page,...)` 子区域上传 + `SetAllocator` 注入

## 4. 验证与收尾

- [x] 4.1 `cmake --build` AuroraCore 通过
- [x] 4.2 `AuroraCoreTest` 全绿
- [ ] 4.3 `openspec archive aurora-render-textures` 归档（需用户确认）
