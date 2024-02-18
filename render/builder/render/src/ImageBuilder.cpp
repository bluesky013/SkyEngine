//
// Created by Zach Lee on 2023/2/26.
//

#include <builder/render/ImageBuilder.h>
#include <builder/render/ImageCompressor.h>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <rhi/Decode.h>
#include <rhi/DDS.h>
#include <core/file/FileIO.h>
#include <core/hash/Hash.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <filesystem>

namespace sky::builder {

    Uuid CreateBufferID(const Uuid &imageId)
    {
        uint32_t seed = 0;
        HashCombine32(seed, imageId.u32[0]);
        HashCombine32(seed, imageId.u32[1]);
        HashCombine32(seed, imageId.u32[2]);
        HashCombine32(seed, imageId.u32[3]);
        return Uuid::CreateWithSeed(seed);
    }

    ImageBuilder::ImageBuilder()
    {
        InitializeCompressor();
    }

    void ImageBuilder::RequestDDS(const BuildRequest &request, BuildResult &result)
    {
        std::vector<uint8_t> data;
        ReadBin(request.fullPath, data);

        rhi::Image::Descriptor imageDesc = {};
        uint32_t offset = rhi::ProcessDDSHeader(data.data(), data.size(), imageDesc);

        auto asset = AssetManager::Get()->CreateAsset<Texture>(request.uuid);
        auto &imageData = asset->Data();

        imageData.format = imageDesc.format;
        imageData.width = imageDesc.extent.width;
        imageData.height = imageDesc.extent.height;
        imageData.depth = imageDesc.extent.depth;
        imageData.arrayLayers = imageDesc.arrayLayers;
        imageData.mipLevels = imageDesc.mipLevels;
        imageData.type = imageData.arrayLayers == 6 ? TextureType::TEXTURE_CUBE : TextureType::TEXTURE_2D;
        imageData.dataSize = static_cast<uint32_t>(data.size() - offset);
        imageData.bufferID = CreateBufferID(request.uuid);

        auto buffer = AssetManager::Get()->CreateAsset<Buffer>(imageData.bufferID);
        auto &bufferData = buffer->Data();
        bufferData.rawData.resize(imageData.dataSize);

        memcpy(bufferData.rawData.data(), data.data() + offset, imageData.dataSize);

        result.products.emplace_back(BuildProduct{KEY.data(), asset});
        result.products.emplace_back(BuildProduct{"GFX_BUFFER", buffer});
        result.success = true;
    }

    void ImageBuilder::RequestSTB(const BuildRequest &request, BuildResult &result)
    {
        int x;
        int y;
        int n;
        int request_comp = compress ? 0 : 4;

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
        imageData.type = TextureType::TEXTURE_2D;
        imageData.bufferID = CreateBufferID(request.uuid);

        auto buffer = AssetManager::Get()->CreateAsset<Buffer>(imageData.bufferID);
        auto &bufferData = buffer->Data();

        BufferImageInfo info = {};
        info.width = imageData.width;
        info.height = imageData.height;
        info.stride = imageData.width * n;

        if (compress) {
            CompressOption option = {};
            option.quality      = Quality::SLOW;
            option.targetFormat = rhi::PixelFormat::BC7_UNORM_BLOCK;
            option.hasAlpha     = n == 4;
            imageData.format    = option.targetFormat;
            CompressImage(data, info, bufferData.rawData, option);

            imageData.dataSize = static_cast<uint32_t>(bufferData.rawData.size());
        } else {
            imageData.format = rhi::PixelFormat::RGBA8_UNORM;

            imageData.dataSize = imageData.width * imageData.height * 4;
            bufferData.rawData.resize(imageData.dataSize);
            memcpy(bufferData.rawData.data(), data, imageData.dataSize);
        }

        result.products.emplace_back(BuildProduct{KEY.data(), asset});
        result.products.emplace_back(BuildProduct{"GFX_BUFFER", buffer});
        result.success = true;

        stbi_image_free(data);
    }

    void ImageBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        if (request.ext == ".dds") {
            RequestDDS(request, result);
        } else {
            RequestSTB(request, result);
        }
    }
}
