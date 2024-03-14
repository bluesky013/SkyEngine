//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/serialization/BinaryArchive.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <render/RHI.h>

namespace sky {
    void ImageAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(format);
        archive.LoadValue(type);
        archive.LoadValue(width);
        archive.LoadValue(height);
        archive.LoadValue(depth);
        archive.LoadValue(mipLevels);
        archive.LoadValue(arrayLayers);
        archive.LoadValue(dataSize);
        archive.LoadValue(reinterpret_cast<char*>(&bufferID), sizeof(Uuid));
    }

    void ImageAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(format);
        archive.SaveValue(type);
        archive.SaveValue(width);
        archive.SaveValue(height);
        archive.SaveValue(depth);
        archive.SaveValue(mipLevels);
        archive.SaveValue(arrayLayers);
        archive.SaveValue(dataSize);
        archive.SaveValue(reinterpret_cast<const char*>(&bufferID), sizeof(Uuid));
    }

    std::shared_ptr<Texture> CreateTexture(const ImageAssetData &data)
    {
        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
        auto bufferPath = AssetManager::Get()->GetAssetPath(data.bufferID);
        if (data.type == TextureType::TEXTURE_2D) {
            auto texture2D = std::make_shared<Texture2D>();
            texture2D->Init(data.format, data.width, data.height, data.mipLevels);
            auto handle = texture2D->Upload(bufferPath, *queue, sizeof(uint32_t));
            queue->Wait(handle);
            return texture2D;
        }

        if (data.type == TextureType::TEXTURE_CUBE) {
            auto textureCube = std::make_shared<TextureCube>();
            textureCube->Init(data.format, data.width, data.height, data.mipLevels);
            return textureCube;
        }

//        queue->Wait(texture2D->Upload(data.bufferAsset->GetPath(), *queue, data.bufferAsset->Data().GetDataOffset()));
        return {};
    }
} // namespace sky