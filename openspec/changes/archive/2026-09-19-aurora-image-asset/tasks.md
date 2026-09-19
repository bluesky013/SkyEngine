## 1. ImageAssetData 结构

- [x] 1.1 定义 `ImageAssetType { TEXTURE_2D, TEXTURE_2D_ARRAY, TEXTURE_3D, TEXTURE_CUBE }` 与 `ImageSliceHeader{offset,size,mipLevel,layer,depth}`
- [x] 1.2 重写 `ImageAssetData`：`version`（`CURRENT_VERSION`）+ `type` + `format(PixelFormat)` + `width/height/depth/mipLevels/arrayLayers` + `slices` + `rawData`
- [x] 1.3 重写 `Save`/`Load`（先写 `version`；`Load` 校验版本，不匹配则拒绝并记录）

## 2. CreateTextureFromAsset

- [x] 2.1 新增 `CreateTextureFromAsset(Device*, Asset<Texture>&)`: 按 `ImageAssetType` 建 `Image::Descriptor`
- [x] 2.2 创建 `Texture2D` / `Texture2DArray` / `Texture3D` / `TextureCube` 并 `Init`（cube 校验 `arrayLayers == 6`）
- [x] 2.3 遍历 `slices`，以 `ImageUploadRequest{mipLevel, layer, imageExtent}` 上传 `rawData` 区段
- [x] 2.4 无效输入（空 device / 版本不匹配 / cube layer 非 6）返回空 `CounterPtr`

## 3. 接入与验证

- [x] 3.1 `ImageAssetData` 的注册（`AuroraReflection` 的 `ImageAssetData` BinLoad/BinSave）保持可用
- [x] 3.2 单测：2D / 2DArray / 3D / Cube 的 Save→Load 往返一致（`AuroraAdaptorTest`）
- [x] 3.3 单测：`version` 不匹配时 `Load` 拒绝
- [x] 3.4 构建 `Aurora.Adaptor` / `AuroraRender` / `Launcher` / `AuroraAdaptorTest` 通过；`openspec validate aurora-image-asset --strict` 通过
