//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/BufferAsset.h>
#include <render/resource/Texture.h>
#include <rhi/Core.h>
#include <string>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct ImageAssetHeader {
        rhi::PixelFormat format  = rhi::PixelFormat::UNDEFINED;
        TextureType type = TextureType::TEXTURE_2D;
        uint32_t width       = 1;
        uint32_t height      = 1;
        uint32_t depth       = 1;
        uint32_t mipLevels   = 1;
        uint32_t arrayLayers = 1;
        uint32_t dataSize    = 1;
    };

    struct ImageAssetData : public ImageAssetHeader {
        Uuid bufferID;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    std::shared_ptr<Texture> CreateTexture(const ImageAssetData &data);

    template <>
    struct AssetTraits<Texture> {
        using DataType                                = ImageAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Texture";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

        static std::shared_ptr<Texture> CreateFromData(const DataType &data)
        {
            return CreateTexture(data);
        }
    };
    using ImageAssetPtr = std::shared_ptr<Asset<Texture>>;
}