//
// Created by Zach Lee on 2023/2/26.
//

#include <builder/render/ImageBuilder.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <builder/render/ImageCompressor.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <filesystem>

namespace sky::builder {

    ImageBuilder::ImageBuilder()
    {
        InitializeCompressor();
    }

    void ImageBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        if (!request.buildKey.empty() && request.buildKey != std::string(KEY)) {
            return;
        }
        int x;
        int y;
        int n;
        unsigned char *data = stbi_load(request.fullPath.c_str(), &x, &y, &n, 0);
        if (data == nullptr) {
            return;
        }
        AssetManager *am = AssetManager::Get();
        std::filesystem::path outPath(request.outDir);
        outPath.append("images");
        std::filesystem::create_directories(outPath);
        outPath.append(request.name);
        outPath.replace_extension(".image");

        auto asset = am->CreateAsset<Texture>(outPath.make_preferred().string());
        auto &imageData = asset->Data();
        imageData.width = static_cast<uint32_t>(x);
        imageData.height = static_cast<uint32_t>(y);
        imageData.format = n == 4 ? rhi::PixelFormat::RGBA8_UNORM : rhi::PixelFormat::RGB8_UNORM;

        outPath.replace_extension(".bin");
        imageData.bufferAsset = am->CreateAsset<Buffer>(outPath.make_preferred().string());
        auto &binaryData = imageData.bufferAsset->Data();

        BufferImageInfo info = {};
        info.width = imageData.width;
        info.height = imageData.height;
        info.stride = imageData.width * n;

        CompressOption option = {};
        option.quality = Quality::FAST;
        option.targetFormat = rhi::PixelFormat::BC7_UNORM_BLOCK;

        imageData.format = option.targetFormat;

        CompressImage(data, info, binaryData.rawData, option);

        result.products.emplace_back(BuildProduct{KEY.data(), asset->GetUuid()});
        am->SaveAsset(imageData.bufferAsset);
        am->SaveAsset(asset);

        stbi_image_free(data);
    }
}
