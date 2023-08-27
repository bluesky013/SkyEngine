//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <rhi/Core.h>
#include <string>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct ImageAssetData {
        rhi::PixelFormat format = rhi::PixelFormat::UNDEFINED;
        uint32_t width       = 1;
        uint32_t height      = 1;
        uint32_t mipLevels   = 1;
        uint32_t arrayLayers = 1;
        std::vector<std::vector<uint8_t>> rawData;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    class Image {
        Image() = default;
        ~Image() = default;
    };

    template <>
    struct AssetTraits<Image> {
        using DataType                                = ImageAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("E28E41C7-FC98-47B9-B86E-42CD0541A4BF");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using ImageAssetPtr = std::shared_ptr<Asset<Image>>;
}