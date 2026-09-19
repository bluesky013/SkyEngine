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

- [x] 4.1 `LoadStbImage`（`ImageSource.cpp`）：`.png`/`.jpg`/`.jpeg` 经 `stbi_load_from_memory` 解码为 RGBA8，`.hdr` 经 `stbi_loadf_from_memory` 解码为 RGBA32F（`StbImageImpl.cpp` 提供 stb 实现 TU）
- [x] 4.2 `LoadKtx`（`ImageSource.cpp`）：解析 KTX1（`glInternalFormat`）与 KTX2（`vkFormat`），映射到 `sky::aurora::PixelFormat`（RGBA8/BC1-7/ASTC 4x4/8x8/10x10/12x12）
- [x] 4.3 处理 mip / `numberOfArrayElements` / `numberOfFaces` / `pixelDepth`，推导 2D / Cube / 2D Array / 3D；块压缩载荷填入 `CookImageSource::asset` 原样透传
- [x] 4.4 KTX2 `supercompressionScheme != 0` 明确拒绝并 `LOG_E`，不产出资产

## 5. 压缩

- [x] 5.1 ASTC 路径（`ImageCompressor.cpp`）：封装 astc-encoder（`astcenc_config_init` / `astcenc_context_alloc` / `astcenc_compress_image` / `astcenc_context_free`），支持 4x4/8x8 UNORM + SRGB，`Quality` 映射 `ASTCENC_PRE_*`
- [x] 5.2 BC7 路径：`DynamicModule("ispc_texcomp")` + `CompressBlocksBC7`（`GetProfile[_alpha]_*` 按 quality + alpha 选择）
- [x] 5.3 `ImageCompressor::DoWork` 按目标 `sky::aurora::PixelFormat` 分派；块对齐 + 边缘复制；非块格式直接跳过（builder 走未压缩输出）

## 6. AuroraImageBuilder

- [x] 6.1 `AuroraImageBuilder : AssetBuilder`：`GetExtensions` = `{.png,.jpg,.jpeg,.hdr,.ktx,.image}`，`QueryType` = `AssetTraits<aurora::Texture>::ASSET_TYPE`
- [x] 6.2 `Request` 分派：`.image` 读 aurora 资产头（type/deps）后 `ImageAssetData::Load`；`.ktx` 走 `LoadKtx`（含压缩透传）；其余走 stb 解码
- [x] 6.3 输出映射（`ImageAssetWriter.h/.cpp`）：`WriteImageAsset(ImageObject|CompressedImage)` 填 `ImageAssetData`（含 slice 表），支持 2D / Cube(6) / 2D Array / 3D
- [x] 6.4 `FindOrCreateAsset<Texture>(uuid)` → 写 `Data()` → `SaveAsset(asset, resolvedBundle)`；管线：resize → linearize → mip → 反向 gamma → 压缩（2D）/ 未压缩

## 7. 配置与框架

- [x] 7.1 新增 `configs/image_build_presets.json` + `engine/configs/` 镜像（`defaultBundle` + `bundles` → `{encode,srgb,quality,block,maxSize,generateMip}`；`common`=NONE / `tex_pc`=BC7 / `tex_mobile`=ASTC）
- [x] 7.2 `ImageBuildPresets::LoadJson` 解析为 `bundle → ImageBuildConfig`；`Resolve` 用 `request.target`（已知 bundle）否则 `defaultBundle`
- [x] 7.3 修复 `engine/framework/src/asset/AssetManager.cpp` 的 `GetBundle`：`!=` 改为 `==`，命名 bundle 精确匹配，`target` 为空返回 `bundles[0]`
- [ ] 7.4 `engine/editor/...EditorApplication.cpp` 加载 `Aurora.Cook`（按用户要求本轮跳过编辑器接入）

## 8. 测试

- [x] 8.1 新建 `engine/aurora/cook/test/`（`AuroraCookTest`）：`GetMipLevel`/格式 helper、U8/Float/HALF 像素往返、`CreateFromImage`/Cube/`CompressedImage`、Kaiser+Box+Lanczos3 mip 链、Lanczos3 核值、HALF 源滤波、Box 2x 对齐、resize 上限/免裁剪、gamma 转换/恒等（18 个用例已通过）
- [x] 8.2 ASTC 与 BC7 编码（`CookCompressTest`）：4x4/8x8 块对齐尺寸（8x8 BC7=64B、9x9 ASTC=144B、8x8 ASTC=16B）与 `PixelFormat` 正确；BC7 经 ispc 运行时 DLL 实际编码
- [x] 8.3 KTX（`CookSourceTest`）：KTX1 / KTX2 解析（最小头 + 数据 fixture）与 `supercompressionScheme != 0` 拒绝
- [x] 8.4 输出映射与 bundle 选择（`CookAssetWriterTest` + `CookConfigTest`）：2D/Cube/压缩 → `ImageAssetData` 切片；`image_build_presets.json` 解析与 `request.target`/`defaultBundle` 解析
- [ ] 8.5 `GetBundle`：改为精确匹配（已修）；未加直接单测（`GetBundle` 为私有，需假 bundle + 文件系统）。注：`FrameworkTest.AssetManagerTest.BuilderTest` 的失败为本轮改动前既存（已用 stash 对照确认）
- [x] 8.6 `AuroraCookTest` 通过 `sky_add_test` + GLOB `test/*` 接入 `SKY_BUILD_TEST`（复用 `../rhi/test/main.cpp`）

## 9. 验证

- [x] 9.1 bootstrap 已完成（astc 5.7.0），`cmake -S . -B build` 复用既有 cache（`SKY_BUILD_TOOL=ON`）通过
- [x] 9.2 构建 `AuroraCook.Static` + `Aurora.Cook`（Debug）编译+链接通过，产出 `Aurora.Cook.dll`
- [x] 9.3 `AuroraCookTest` **28/28 通过**（含 8.2/8.3/8.4 新增 10 个用例）
- [x] 9.4 人工核对（clang-format 不可用）：新增文件全 ASCII，指针右对齐/4 空格/函数换行符合 `.clang-format`
