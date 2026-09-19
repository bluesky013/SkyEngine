//
// Aurora image asset: CPU payload (image descriptor + tightly-packed mip-0
// bytes) for aurora::Texture.
//

#pragma once

#include <aurora/resource/Texture.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace sky::aurora {

    struct ImageAssetData {
        uint32_t             width       = 0;
        uint32_t             height      = 0;
        uint32_t             mipLevels   = 1;
        uint32_t             arrayLayers = 1;
        uint32_t             format      = 0;
        std::vector<uint8_t> pixels;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(width);
            ar.SaveValue(height);
            ar.SaveValue(mipLevels);
            ar.SaveValue(arrayLayers);
            ar.SaveValue(format);
            ar.SaveValue(static_cast<uint32_t>(pixels.size()));
            if (!pixels.empty()) {
                ar.SaveValue(reinterpret_cast<const char *>(pixels.data()), pixels.size());
            }
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(width);
            ar.LoadValue(height);
            ar.LoadValue(mipLevels);
            ar.LoadValue(arrayLayers);
            ar.LoadValue(format);
            uint32_t size = 0;
            ar.LoadValue(size);
            pixels.resize(size);
            if (size != 0) {
                ar.LoadValue(reinterpret_cast<char *>(pixels.data()), size);
            }
        }
    };

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::Texture> {
        using DataType                                = sky::aurora::ImageAssetData;
        static constexpr std::string_view ASSET_TYPE  = "AuroraTexture";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
