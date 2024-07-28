//
// Created by Zach Lee on 2023/2/26.
//

#include <builder/render/ImageBuilder.h>
#include <builder/render/ImageCompressor.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>
#include <rhi/Decode.h>
#include <core/file/FileIO.h>
#include <core/hash/Hash.h>
#include <core/archive/MemoryArchive.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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

    void ImageBuilder::RequestDDS(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        std::vector<uint8_t> data;
        request.file->ReadBin(data);

        rhi::Image::Descriptor imageDesc = {};
        uint32_t offset = rhi::ProcessDDSHeader(data.data(), data.size(), imageDesc);

        auto asset = AssetManager::Get()->FindOrCreateAsset<Texture>(request.assetInfo->uuid);
        auto &imageData = asset->Data();

        imageData.format = imageDesc.format;
        imageData.width = imageDesc.extent.width;
        imageData.height = imageDesc.extent.height;
        imageData.depth = imageDesc.extent.depth;
        imageData.arrayLayers = imageDesc.arrayLayers;
        imageData.mipLevels = imageDesc.mipLevels;
        imageData.type = imageData.arrayLayers == 6 ? TextureType::TEXTURE_CUBE : TextureType::TEXTURE_2D;

//        auto buffer = AssetManager::Get()->FindOrCreateAsset<Buffer>(imageData.bufferID);
//        auto &bufferData = buffer->Data();
//        bufferData.rawData.resize(imageData.dataSize);
//        memcpy(bufferData.rawData.data(), data.data() + offset, imageData.dataSize);

        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    void ImageBuilder::RequestSTB(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        int x;
        int y;
        int n;
        int request_comp = 4;

        std::vector<uint8_t> rawData;
        request.file->ReadBin(rawData);

        unsigned char *data = nullptr;
        data = stbi_load_from_memory(static_cast<const stbi_uc*>(rawData.data()), static_cast<int>(rawData.size()), &x, &y, &n, request_comp);
        if (data == nullptr) {
            return;
        }
        auto *am = AssetManager::Get();

        auto asset = am->FindOrCreateAsset<Texture>(request.assetInfo->uuid);
        auto &imageData = asset->Data();
        imageData.width = static_cast<uint32_t>(x);
        imageData.height = static_cast<uint32_t>(y);
        imageData.type = TextureType::TEXTURE_2D;

        if (compress) {
            CompressOption option = {};
            option.quality      = Quality::SLOW;
            option.targetFormat = rhi::PixelFormat::BC7_UNORM_BLOCK;
            option.hasAlpha     = true;
            imageData.format    = option.targetFormat;

            std::vector<uint8_t> compressedData;

            BufferImageInfo info = {};
            info.width = imageData.width;
            info.height = imageData.height;
            info.stride = imageData.width * 4;

            CompressImage(data, info, compressedData, option);

            ImageSliceHeader slice = {};
            slice.offset = 0;
            slice.size = static_cast<uint32_t>(compressedData.size());

            imageData.dataSize += slice.size;
            imageData.slices.emplace_back(slice);

            imageData.rawData.storage.resize(imageData.dataSize);
            memcpy(imageData.rawData.storage.data(), compressedData.data(), imageData.dataSize);
        } else {
            imageData.format = rhi::PixelFormat::RGBA8_UNORM;

            ImageSliceHeader slice = {};
            slice.offset = 0;
            slice.size = imageData.width * imageData.height * 4;;

            imageData.dataSize += slice.size;
            imageData.slices.emplace_back(slice);

            imageData.rawData.storage.resize(imageData.dataSize);
            memcpy(imageData.rawData.storage.data(), reinterpret_cast<const char *>(data), imageData.dataSize);
        }

        AssetManager::Get()->SaveAsset(asset, request.target);
        stbi_image_free(data);
    }

    void ImageBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        if (request.assetInfo->ext == ".dds") {
            RequestDDS(request, result);
        } else {
            RequestSTB(request, result);
        }
    }

    void ImageBuilder::LoadConfig(const FileSystemPtr &cfg)
    {
    }
}
