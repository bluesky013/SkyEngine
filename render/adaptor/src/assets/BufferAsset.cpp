//
// Created by Zach Lee on 2023/9/2.
//

#include <render/adaptor/assets/BufferAsset.h>

namespace sky {

    void BufferAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(size);
    }

    void BufferAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(rawData.size()));
        archive.SaveValue(reinterpret_cast<const char*>(rawData.data()), static_cast<uint32_t>(rawData.size()));
    }
} // namespace sky