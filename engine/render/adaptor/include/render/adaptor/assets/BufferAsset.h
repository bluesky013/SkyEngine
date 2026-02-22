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

    struct BufferAssetHeader {
        uint32_t dataSize;
    };

    struct BufferAssetData : public BufferAssetHeader {
        std::vector<uint8_t> rawData;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Buffer> {
        using DataType                                = BufferAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Buffer";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using BufferAssetPtr = std::shared_ptr<Asset<Buffer>>;
} // namespace sky