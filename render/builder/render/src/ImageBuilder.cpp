//
// Created by Zach Lee on 2023/2/26.
//

#include <builder/render/ImageBuilder.h>
#include <builder/render/ImageCompressor.h>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/ImageAsset.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <filesystem>

namespace sky::builder {

    constexpr bool COMPRESS = false;

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
        int request_comp = COMPRESS ? 0 : 4;

        unsigned char *data = nullptr;
        if (request.rawData != nullptr) {
            data = stbi_load_from_memory(static_cast<const stbi_uc*>(request.rawData), request.dataSize, &x, &y, &n, request_comp);
        } else {
            data = stbi_load(request.fullPath.c_str(), &x, &y, &n, request_comp);
        }
        if (data == nullptr) {
            return;
        }
        auto *am = AssetManager::Get();

        auto asset = am->CreateAsset<Texture>(request.uuid);
        auto &imageData = asset->Data();
        imageData.width = static_cast<uint32_t>(x);
        imageData.height = static_cast<uint32_t>(y);

        imageData.bufferAsset = am->CreateAsset<Buffer>(AssetManager::GetUUIDByPath(request.relativePath + "/data"));
        auto &binaryData = imageData.bufferAsset->Data();

        BufferImageInfo info = {};
        info.width = imageData.width;

        info.height = imageData.height;
        info.stride = imageData.width * n;

        if (COMPRESS) {
            CompressOption option = {};
            option.quality        = Quality::SLOW;
            option.targetFormat   = rhi::PixelFormat::BC7_UNORM_BLOCK;
            option.hasAlpha       = n == 4;
            imageData.format      = option.targetFormat;
            CompressImage(data, info, binaryData.rawData, option);
        } else {
            imageData.format = rhi::PixelFormat::RGBA8_UNORM;
            binaryData.rawData.resize(info.width * info.height * 4);
            memcpy(binaryData.rawData.data(), data, binaryData.rawData.size());
        }

        result.products.emplace_back(BuildProduct{KEY.data(), asset});
        result.products.emplace_back(BuildProduct{KEY.data(), asset});

        stbi_image_free(data);
    }
}
