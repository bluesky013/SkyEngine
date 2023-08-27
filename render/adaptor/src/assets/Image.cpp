//
// Created by Zach Lee on 2023/2/26.
//

#include <render/adaptor/assets/Image.h>
#include <framework/serialization/BinaryArchive.h>

namespace sky {
    void ImageAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(format);
        archive.LoadValue(width);
        archive.LoadValue(height);
        archive.LoadValue(mipLevels);
        archive.LoadValue(arrayLayers);
    }

    void ImageAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(format);
        archive.SaveValue(width);
        archive.SaveValue(height);
        archive.SaveValue(mipLevels);
        archive.SaveValue(arrayLayers);
        archive.SaveValue(static_cast<uint32_t>(rawData.size()));
        for (auto &data : rawData) {
            archive.SaveValue(static_cast<uint32_t>(data.size()));
            archive.SaveValue(reinterpret_cast<const char*>(data.data()), static_cast<uint32_t>(data.size()));
        }
    }


}