//
// Created by Zach Lee on 2023/9/2.
//

#include <render/adaptor/assets/BufferAsset.h>

namespace sky {

    void BufferAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(usage.value);
    }

    void BufferAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(usage.value);
        archive.SaveValue(static_cast<uint32_t>(rawData.size()));
        for (const auto &data : rawData) {
            archive.SaveValue(static_cast<uint32_t>(data.size()));
            archive.SaveValue(reinterpret_cast<const char*>(data.data()), static_cast<uint32_t>(data.size()));
        }
    }
} // namespace sky