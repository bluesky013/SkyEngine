//
// Created by Zach Lee on 2023/2/26.
//

#include <builder/render/ImageBuilder.h>
#include <builder/render/image/ImageCompressor.h>
#include <builder/render/image/ImageConverter.h>
#include <builder/render/image/ImageMipGen.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>
#include <rhi/Decode.h>
#include <core/hash/Hash.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>


namespace sky::builder {
    static rhi::PixelFormat GetPixelFormat(PixelType type, uint32_t components)
    {
        if (type == PixelType::U8) {
            SKY_ASSERT(components == 4);
            return rhi::PixelFormat::RGBA8_UNORM;
        } else if (type == PixelType::Float) {
            return static_cast<rhi::PixelFormat>(static_cast<uint32_t>(rhi::PixelFormat::R32_SFLOAT) + components - 1);
        }
        return rhi::PixelFormat::UNDEFINED;
    }

    static void SaveImageData(const CompressedImagePtr& image, ImageAssetData &imageData)
    {
        imageData.format = image->format;
        imageData.dataSize = 0;
        imageData.mipLevels = static_cast<uint32_t>(image->mips.size());

        for (uint32_t i = 0; i < image->mips.size(); ++i) {
            const auto &mipData = image->mips[i];

            ImageSliceHeader slice = {};
            slice.offset = imageData.dataSize;
            slice.size = mipData.dataLength;
            slice.mipLevel = i;

            imageData.dataSize += slice.size;
            imageData.slices.emplace_back(slice);

            imageData.rawData.storage.resize(imageData.dataSize);
            memcpy(imageData.rawData.storage.data() + slice.offset, mipData.data.get(), mipData.dataLength);
        }
    }

    static void SaveImageData(const ImageObjectPtr & image, ImageAssetData &imageData)
    {
        uint32_t baseMip = 0;

        imageData.format = GetPixelFormat(image->pixelType, image->components);
        imageData.dataSize = 0;
        imageData.mipLevels = static_cast<uint32_t>(image->mips.size()) - baseMip;

        imageData.width = imageData.width >> baseMip;
        imageData.height = imageData.height >> baseMip;

        for (uint32_t i = baseMip; i < image->mips.size(); ++i) {
            const auto &mipData = image->mips[i];

            ImageSliceHeader slice = {};
            slice.offset = imageData.dataSize;
            slice.size = mipData.dataLength;
            slice.mipLevel = i - baseMip;

            imageData.dataSize += slice.size;
            imageData.slices.emplace_back(slice);

            imageData.rawData.storage.resize(imageData.dataSize);
            memcpy(imageData.rawData.storage.data() + slice.offset, mipData.data.get(), mipData.dataLength);
        }
    }

    void ProcessDDS(ImageAssetData& imageData, uint8_t *input, uint64_t size)
    {
        rhi::Image::Descriptor imageDesc = {};
        uint32_t dataOffset = ProcessDDSHeader(input, size, imageDesc);

        auto* fmtInfo = rhi::GetImageInfoByFormat(imageDesc.format);
        if (fmtInfo == nullptr) {
            return;
        }

        uint8_t *dataStart = input + dataOffset;
        uint32_t offset = 0;

        imageData.format = imageDesc.format;
        imageData.width = imageDesc.extent.width;
        imageData.height = imageDesc.extent.height;
        imageData.depth = imageDesc.extent.depth;
        imageData.arrayLayers = imageDesc.arrayLayers;
        imageData.mipLevels = imageDesc.mipLevels;
        imageData.type = imageData.arrayLayers == 6 ? TextureType::TEXTURE_CUBE : TextureType::TEXTURE_2D;

        uint32_t blockWidth = fmtInfo->blockWidth;
        uint32_t blockHeight = fmtInfo->blockHeight;
        uint32_t blockSize = fmtInfo->blockSize;

        for (uint32_t j = 0; j < imageDesc.arrayLayers; ++j) {
            for (uint32_t i = 0; i < imageDesc.mipLevels; ++i) {
                auto width  = std::max(imageDesc.extent.width >> i, 1U);
                auto height = std::max(imageDesc.extent.height >> i, 1U);

                uint32_t rowLength   = (width + blockWidth - 1) / blockWidth;
                uint32_t imageHeight = (height + blockHeight - 1) / blockHeight;
                uint32_t currentSize = rowLength * imageHeight * blockSize;

                ImageSliceHeader header = {};
                header.offset = offset;
                header.mipLevel = i;
                header.layer = j;
                header.size = currentSize;

                imageData.slices.emplace_back(header);
                offset += currentSize;
            }
        }

        imageData.dataSize = offset;
        imageData.rawData.storage.resize(offset);
        memcpy(imageData.rawData.storage.data(), dataStart, offset);
    }


    ImageBuilder::ImageBuilder()
    {
        InitializeCompressor();
    }

    void ImageBuilder::RequestImage(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        auto archive = request.file->ReadAsArchive();
        BinaryInputArchive bin(*archive);

        auto  asset = AssetManager::Get()->FindOrCreateAsset<Texture>(request.assetInfo->uuid);
        auto &imageData = asset->Data();

        imageData.Load(bin);

        imageData.rawData.storage.resize(imageData.dataSize);
        bin.LoadValue(reinterpret_cast<char*>(imageData.rawData.storage.data()), imageData.dataSize);

        AssetManager::Get()->SaveAsset(asset, request.target);
    }

    void ImageBuilder::RequestDDS(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        std::vector<uint8_t> data;
        request.file->ReadBin(data);

        auto asset = AssetManager::Get()->FindOrCreateAsset<Texture>(request.assetInfo->uuid);
        auto &imageData = asset->Data();

        ProcessDDS(imageData, data.data(), data.size());
        AssetManager::Get()->SaveAsset(asset, request.target);

        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    void ImageBuilder::RequestSTB(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        int x;
        int y;
        int channel;

        std::vector<uint8_t> rawData;
        request.file->ReadBin(rawData);

        unsigned char *data = nullptr;
        data = stbi_load_from_memory(static_cast<const stbi_uc*>(rawData.data()), static_cast<int>(rawData.size()), &x, &y, &channel, 4);
        if (data == nullptr) {
            return;
        }

        ImageObjectPtr image = ImageObject::CreateImage2D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), PixelType::U8, static_cast<uint32_t>(4));
        image->FillMip0(data, static_cast<uint32_t>(x * y * 4));
        stbi_image_free(data);

        auto config = std::make_unique<ImageBuildConfig>();
        config->generateMip = true;
        config->compress = false;

        CompressedImagePtr compressedImage;

        if (config != nullptr) {

            if (config->generateMip) {
                // convert to linear
                auto linearImage = ImageObject::CreateImage2D(image->width, image->height, PixelType::Float, image->components);
                linearImage->FillMip0();
                {
                    ImageConverter converter(ImageConverter::Payload{image, linearImage, config->isLinear ? 1.f : 2.2f});
                    converter.DoWork();
                }

                // generate mips
                ImageMipGen mipGen(ImageMipGen::Payload{linearImage});
                mipGen.DoWork();

                // recover
                image = ImageObject::CreateFromImage(linearImage, PixelType::U8, 4);
                {
                    ImageConverter converter(ImageConverter::Payload{linearImage, image, config->isLinear ? 1.f : 1.f / 2.2f});
                    converter.DoWork();
                }
            }

            if (config->compress) {
                CompressOption option = {};
                option.quality      = Quality::FAST;
                option.targetFormat = rhi::PixelFormat::BC7_UNORM_BLOCK;
//                option.targetFormat = rhi::PixelFormat::ASTC_8x8_UNORM_BLOCK;
                option.hasAlpha     = channel == 4;

                compressedImage = CompressedImage::CreateFromImageObject(image, option.targetFormat);
                for (uint32_t i = 0; i < image->mips.size(); ++i) {
                    ImageCompressor compressor(ImageCompressor::Payload{image, compressedImage, option, i});
                    compressor.DoWork();
                }
            }
        }


        auto *am = AssetManager::Get();
        auto asset = am->FindOrCreateAsset<Texture>(request.assetInfo->uuid);
        auto &imageData = asset->Data();
        imageData.width = static_cast<uint32_t>(x);
        imageData.height = static_cast<uint32_t>(y);
        imageData.type = TextureType::TEXTURE_2D;

        if (compressedImage) {
            SaveImageData(compressedImage, imageData);
        } else {
            SaveImageData(image, imageData);
        }
        AssetManager::Get()->SaveAsset(asset, request.target);
    }

    void ImageBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        if (request.assetInfo->ext == ".image") {
            RequestImage(request, result);
        } else if (request.assetInfo->ext == ".dds") {
            RequestDDS(request, result);
        } else {
            RequestSTB(request, result);
        }
    }

    void ImageBuilder::LoadConfig(const FileSystemPtr &cfg)
    {
    }
}
