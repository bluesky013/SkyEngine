## 1. 第三方依赖（astc-encoder）

- [x] 1.1 升级 tag 到 `5.7.0`，运行 `python python/third_party.py -p Win32 -t astc`，确认 `build_3rd/Win32/astc/{include,lib}` 真实产出；修正 `cmake/thirdparty.json` 的 astc 条目（`ASTCENC_SHAREDLIB=OFF`、`ASTCENC_DECOMPRESSOR=OFF`、`ASTCENC_INVARIANCE=ON`、`ASTCENC_UNIVERSAL_BUILD=OFF`）并加 `cmake/patches/astc.patch`（补静态库 + 头文件的 install），已重跑并安装成功
- [x] 1.2 新建 `cmake/thirdparty/Findastc.cmake`：`sky_3rd_static(astc LIBS astcenc-native-static)`（默认 `native` ISA 产物），导出 `3rdParty::astc`；已用临时工程编译+运行验证（`astcenc_config_init` 返回成功）
- [x] 1.3 在 `engine/aurora/cook/image/CMakeLists.txt` `sky_find_3rd(TARGET astc DIR astc)` 并链接 `3rdParty::astc`；`sky_find_3rd(TARGET ispc_texcomp DIR ispc_texcomp)` 作为 BC 路径（顺带给 `Findispc_texcomp.cmake` 补 target 重复定义守卫）

## 2. cook 模块骨架

- [x] 2.1 新建 `engine/aurora/cook/image/` → `AuroraCook.Static`（STATIC，GLOB `src/*` + `include/*`，`LINK_LIBS Aurora.Adaptor Framework Core 3rdParty::stb 3rdParty::ispc_texcomp 3rdParty::astc`）
- [x] 2.2 新建 `engine/aurora/cook/module/AuroraCookModule.cpp` + `engine/aurora/cook/CMakeLists.txt` → `Aurora.Cook`（SHARED，`add_subdirectory(image)`，`SOURCES module/AuroraCookModule.cpp`，`LINK_LIBS AuroraCook.Static`）
- [x] 2.3 `AuroraCookModule : IModule` + `REGISTER_MODULE(sky::aurora::AuroraCookModule)`；`Init` 调 `sky::aurora::AuroraReflection(SerializationContext::Get())`（幂等）后 `AssetBuilderManager::RegisterBuilder(new AuroraImageBuilder())`（builder 目前为 stub：Extensions + QueryType，Request 记录未实现）
- [x] 2.4 `sky_add_dependency(TARGET Aurora.Cook DEPENDENCIES Launcher Editor)`
- [x] 2.5 `engine/aurora/CMakeLists.txt` 增加 `if (SKY_BUILD_TOOL AND (Win32/macOS)) add_subdirectory(cook) endif()`

## 3. 图像处理移植（aurora 命名空间 / 格式）

- [x] 3.1 移植 `ImageObject` / `ImageMipData` / `CompressedImage` 与 `GetMipLevel` 等 helper 到 `sky::aurora::cook`，格式改用 `sky::aurora::PixelFormat`（`ImageProcess.h/.cpp`；含 U8/HALF/Float 像素读写，HALF 用内置 half<->float）
- [x] 3.2 移植 `ImageFilter`（`Box`/`Kaiser` 卷积核 + 共用 `PolyphaseKernel` + `MakeFilter` 工厂）/ `ImageMipGen` / `ImageResizer` / `ImageConverter`
- [x] 3.3 组件数量与字节大小走 `aurora/rhi/Core.h` 的 `GetImageFormatInfo`（`GetNumComp`/`GetBytePerComp`），无 legacy `rhi::` 引用（未用到 RowPitch/SlicePitch，未引入）
- [x] 3.4 新增 `Lanczos3` 滤波核（`MipGenType::Lanczos3`、`LanczosFilter`），mip/resize 经 `MakeFilter` 分派
- [x] 3.5 修正 legacy 算法缺陷：`PolyphaseKernel` 按每个输出中心的真实距离建权重（消除半像素偏移）；kernel 读取支持 `PixelType::HALF`（16 位源不再变黑）

## 4. 源解码

- [ ] 4.1 `StbImageDecoder`：`.png`/`.jpg`/`.jpeg`/`.hdr` 经 `stbi_load_from_memory` 解码为 RGBA8（HDR 为浮点路径）
- [ ] 4.2 `KtxReader`：解析 KTX1（`«KTX 11»`，`glInternalFormat`）与 KTX2（`«KTX 20»`，`vkFormat`），映射到 `sky::aurora::PixelFormat`
- [ ] 4.3 KtxReader 处理 `numberOfMipmapLevels` / `numberOfArrayElements` / `numberOfFaces` / 3D `pixelDepth`，块压缩载荷原样透传
- [ ] 4.4 KTX2 `supercompressionScheme != 0` 明确拒绝并 `LOG_E`，不产出资产

## 5. 压缩

- [ ] 5.1 `AstcCompressor`：封装 astc-encoder（`astcenc_config_init` / `astcenc_context_alloc` / `astcenc_compress_image` / `astcenc_compress_reset`），支持 ASTC 4x4/8x8 UNORM + SRGB，`Quality` 映射 `ASTCENC_PRE_*`
- [ ] 5.2 `BcCompressor`：复用 `DynamicModule("ispc_texcomp")` + `CompressBlocksBC7`，支持 BC7 UNORM/SRGB（alpha 感知）
- [ ] 5.3 压缩器按目标 `sky::aurora::PixelFormat` 分派（BC*/ASTC*），非块格式走未压缩回退

## 6. AuroraImageBuilder

- [ ] 6.1 `AuroraImageBuilder : AssetBuilder`：`GetExtensions` = `{.png,.jpg,.jpeg,.hdr,.ktx,.image}`，`QueryType` = `AssetTraits<aurora::Texture>::ASSET_TYPE`
- [ ] 6.2 `Request` 分派：`.image` 直读 aurora `ImageAssetData`；其余走解码→缩放→线性化→mip→压缩管线
- [ ] 6.3 输出映射：填 `sky::aurora::ImageAssetData`（`version`/`format`/`type`/尺寸/`mipLevels`/`arrayLayers`/`ImageSliceHeader{offset,size,mipLevel,layer,depth}`/`rawData`），支持 2D / Cube(6) / 2D Array
- [ ] 6.4 `FindOrCreateAsset<aurora::Texture>(uuid)` → `Data()` 写入 → `SaveAsset(asset, resolvedBundle)`

## 7. 配置与框架

- [ ] 7.1 新增 `configs/image_build_presets.json`（`bundles` → `{format,quality,block,maxSize,generateMip}` + `defaultBundle`），并在 `engine/configs/` 放置镜像
- [ ] 7.2 `AuroraImageBuilder::LoadConfig` 读取该文件为 `bundle → ImageBuildConfig`；`Request` 用 `request.target`（已知 bundle）否则 `defaultBundle` 解析输出 bundle
- [ ] 7.3 修复 `engine/framework/src/asset/AssetManager.cpp` 的 `GetBundle`：`!=` 改为 `==`，命名 bundle 精确匹配，`target` 为空返回 `bundles[0]`
- [ ] 7.4 `engine/editor/src/application/EditorApplication.cpp` 在 `SKY_BUILD_TOOL` 下把 `Aurora.Cook`（依赖 `AuroraRender.Editor`）加入模块列表

## 8. 测试

- [x] 8.1 新建 `engine/aurora/cook/test/`（`AuroraCookTest`）：`GetMipLevel`/格式 helper、U8/Float/HALF 像素往返、`CreateFromImage`/Cube/`CompressedImage`、Kaiser+Box+Lanczos3 mip 链、Lanczos3 核值、HALF 源滤波、Box 2x 对齐、resize 上限/免裁剪、gamma 转换/恒等（18 个用例已通过）
- [ ] 8.2 ASTC 与 BC7 编码：块对齐尺寸与 `sky::aurora::PixelFormat` 正确（用合成 RGBA 输入，不依赖真实纹理）
- [ ] 8.3 KtxReader：KTX1/KTX2 解析（构造最小头 + 数据 fixture）与 `supercompressionScheme != 0` 拒绝
- [ ] 8.4 builder 往返：构建 `.png` 得 `ImageAssetData`，`Load` 后字段/切片一致；`request.target` 选 bundle
- [ ] 8.5 `GetBundle`：精确匹配、缺失返回 `nullptr`、空 target 返回首 bundle
- [x] 8.6 `AuroraCookTest` 通过 `sky_add_test` + GLOB `test/*` 接入 `SKY_BUILD_TEST`（复用 `../rhi/test/main.cpp`）

## 9. 验证

- [ ] 9.1 bootstrap（若需要）`python python/third_party.py -p Win32 -j 8`，再 `cmake -S . -B build -DSKY_BUILD_TOOL=ON`（保留既有 cache）
- [ ] 9.2 构建 `AuroraCook.Static` + `Aurora.Cook`（Debug）编译通过
- [ ] 9.3 `SKY_BUILD_TEST=ON` 下运行 `AuroraCookTest`（及其它受影响测试）全部通过
- [ ] 9.4 按 `.clang-format` / `.clang-tidy` 人工核对新增代码（clang-format 不可用时）
