//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/serialization/BinaryArchive.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <render/RHI.h>

namespace sky {
    void ImageAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        archive.LoadValue(format);
        archive.LoadValue(type);
        archive.LoadValue(width);
        archive.LoadValue(height);
        archive.LoadValue(depth);
        archive.LoadValue(mipLevels);
        archive.LoadValue(arrayLayers);

        uint32_t size = 0;
        archive.LoadValue(size);
        slices.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            auto &slice = slices[i];
            archive.LoadValue(slice.offset);
            archive.LoadValue(slice.size);
            archive.LoadValue(slice.mipLevel);
            archive.LoadValue(slice.layer);
        }

        archive.LoadValue(dataSize);
        dataOffset = static_cast<uint32_t>(archive.GetStream().Tell());
    }

    void ImageAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(format);
        archive.SaveValue(type);
        archive.SaveValue(width);
        archive.SaveValue(height);
        archive.SaveValue(depth);
        archive.SaveValue(mipLevels);
        archive.SaveValue(arrayLayers);

        archive.SaveValue(static_cast<uint32_t>(slices.size()));
        for (const auto &slice : slices) {
            archive.SaveValue(slice.offset);
            archive.SaveValue(slice.size);
            archive.SaveValue(slice.mipLevel);
            archive.SaveValue(slice.layer);
        }

        // data size
        archive.SaveValue(dataSize);
        SKY_ASSERT(dataSize == static_cast<uint32_t>(rawData.storage.size()));

        // save raw
        archive.SaveValue(reinterpret_cast<const char*>(rawData.storage.data()), dataSize);
    }

    CounterPtr<Texture> CreateTextureFromAsset(const ImageAssetPtr &asset)
    {
        const auto &data = asset->Data();
        const auto &uuid = asset->GetUuid();

        auto *am = AssetManager::Get();
        auto file = am->OpenFile(uuid);
        SKY_ASSERT(file);

        ImageData imageData;
        auto *fileStream = new rhi::FileStream(file, data.dataOffset);

        for (const auto &slice : data.slices) {
            rhi::ImageUploadRequest request = {};
            request.source = fileStream;
            request.offset = slice.offset;
            request.size = slice.size;
            request.layer = slice.layer;
            request.mipLevel = slice.mipLevel;
            request.imageExtent.width = data.width;
            request.imageExtent.height = data.height;
            request.imageExtent.depth = data.depth;
            imageData.slices.emplace_back(request);
        }

        if (data.type == TextureType::TEXTURE_2D) {
            auto *texture2D = new Texture2D();
            texture2D->Init(data.format, data.width, data.height, data.mipLevels);
            texture2D->SetUploadStream(std::move(imageData));
            return texture2D;
        }

        if (data.type == TextureType::TEXTURE_CUBE) {
            auto *textureCube = new TextureCube();
            textureCube->Init(data.format, data.width, data.height, data.mipLevels);
            textureCube->SetUploadStream(std::move(imageData));
            return textureCube;
        }
        return {};
    }
} // namespace sky