//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <core/archive/StreamArchive.h>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/BufferAsset.h>
#include <render/resource/Texture.h>
#include <rhi/Core.h>
#include <string>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct ImageSliceHeader {
        uint32_t offset   = 0;
        uint32_t size     = 0;
        uint32_t mipLevel = 0;
        uint32_t layer    = 0;
    };

    struct ImageAssetHeader {
        uint32_t version = 0;
        rhi::PixelFormat format  = rhi::PixelFormat::UNDEFINED;
        TextureType type = TextureType::TEXTURE_2D;
        uint32_t width       = 1;
        uint32_t height      = 1;
        uint32_t depth       = 1;
        uint32_t mipLevels   = 1;
        uint32_t arrayLayers = 1;
        std::vector<ImageSliceHeader> slices;
        uint32_t dataSize;
    };

    struct ImageAssetData : public ImageAssetHeader {
        uint32_t dataOffset;
        AssetRawData rawData;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };
    using ImageAssetPtr = std::shared_ptr<Asset<Texture>>;

    template <>
    struct AssetTraits<Texture> {
        using DataType                                = ImageAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Texture";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    CounterPtr<Texture> CreateTextureFromAsset(const ImageAssetPtr &asset);
}