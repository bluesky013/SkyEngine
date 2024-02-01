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
        archive.LoadValue(imageType);
        archive.LoadValue(width);
        archive.LoadValue(height);
        archive.LoadValue(depth);
        archive.LoadValue(mipLevels);
        archive.LoadValue(arrayLayers);
        archive.LoadValue(dataSize);
    }

    void ImageAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(format);
        archive.SaveValue(imageType);
        archive.SaveValue(width);
        archive.SaveValue(height);
        archive.SaveValue(depth);
        archive.SaveValue(mipLevels);
        archive.SaveValue(arrayLayers);
        archive.SaveValue(dataSize);
    }

    std::shared_ptr<Texture> CreateTexture(const ImageAssetData &data)
    {
        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
        auto texture2D = std::make_shared<Texture2D>();
        texture2D->Init(data.format, data.width, data.height, data.mipLevels);
//        queue->Wait(texture2D->Upload(data.bufferAsset->GetPath(), *queue, data.bufferAsset->Data().GetDataOffset()));

        return texture2D;
    }
} // namespace sky