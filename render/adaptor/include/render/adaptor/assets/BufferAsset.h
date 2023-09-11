//
// Created by Zach Lee on 2023/9/2.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <render/resource/Buffer.h>
#include <rhi/Core.h>
#include <string>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct BufferAssetData {
        std::vector<std::vector<uint8_t>> rawData;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Buffer> {
        using DataType                                = BufferAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("C0C7D089-9B8B-4B9E-9D61-915C9380705D");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using BufferAssetPtr = std::shared_ptr<Asset<Buffer>>;
} // namespace sky