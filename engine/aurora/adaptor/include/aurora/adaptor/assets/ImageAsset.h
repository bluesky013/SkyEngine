//
// Aurora image asset: versioned CPU payload for aurora::Texture. Supports 2D,
// 2D array, 3D and cube maps with an explicit slice table (mip / layer / depth).
// The runtime-only build path CreateTextureFromAsset turns it into a Texture.
//

#pragma once

#include <aurora/resource/Texture.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class Device;

    enum class ImageAssetType : uint32_t {
        TEXTURE_2D = 0,
        TEXTURE_2D_ARRAY,
        TEXTURE_3D,
        TEXTURE_CUBE,
    };

    struct ImageSliceHeader {
        uint32_t offset   = 0; // byte offset into rawData
        uint32_t size     = 0; // byte size of the slice
        uint32_t mipLevel = 0;
        uint32_t layer    = 0; // array layer / cube face
        uint32_t depth    = 0; // z slice (3D)
    };

    struct ImageAssetData {
        static constexpr uint32_t CURRENT_VERSION = 1;

        uint32_t                      version     = CURRENT_VERSION;
        PixelFormat                   format      = PixelFormat::UNDEFINED;
        ImageAssetType                type        = ImageAssetType::TEXTURE_2D;
        uint32_t                      width       = 1;
        uint32_t                      height      = 1;
        uint32_t                      depth       = 1;
        uint32_t                      mipLevels   = 1;
        uint32_t                      arrayLayers = 1;
        std::vector<ImageSliceHeader> slices;
        std::vector<uint8_t>          rawData;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(version);
            ar.SaveValue(static_cast<uint32_t>(format));
            ar.SaveValue(static_cast<uint32_t>(type));
            ar.SaveValue(width);
            ar.SaveValue(height);
            ar.SaveValue(depth);
            ar.SaveValue(mipLevels);
            ar.SaveValue(arrayLayers);

            ar.SaveValue(static_cast<uint32_t>(slices.size()));
            for (const auto &slice : slices) {
                ar.SaveValue(slice.offset);
                ar.SaveValue(slice.size);
                ar.SaveValue(slice.mipLevel);
                ar.SaveValue(slice.layer);
                ar.SaveValue(slice.depth);
            }

            ar.SaveValue(static_cast<uint32_t>(rawData.size()));
            if (!rawData.empty()) {
                ar.SaveValue(reinterpret_cast<const char *>(rawData.data()), rawData.size());
            }
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(version);
            if (version != CURRENT_VERSION) {
                clear();
                return;
            }

            uint32_t formatValue = 0;
            uint32_t typeValue   = 0;
            ar.LoadValue(formatValue);
            ar.LoadValue(typeValue);
            format = static_cast<PixelFormat>(formatValue);
            type   = static_cast<ImageAssetType>(typeValue);

            ar.LoadValue(width);
            ar.LoadValue(height);
            ar.LoadValue(depth);
            ar.LoadValue(mipLevels);
            ar.LoadValue(arrayLayers);

            uint32_t sliceCount = 0;
            ar.LoadValue(sliceCount);
            slices.resize(sliceCount);
            for (auto &slice : slices) {
                ar.LoadValue(slice.offset);
                ar.LoadValue(slice.size);
                ar.LoadValue(slice.mipLevel);
                ar.LoadValue(slice.layer);
                ar.LoadValue(slice.depth);
            }

            uint32_t rawSize = 0;
            ar.LoadValue(rawSize);
            rawData.resize(rawSize);
            if (rawSize != 0) {
                ar.LoadValue(reinterpret_cast<char *>(rawData.data()), rawSize);
            }
        }

        void clear()
        {
            format      = PixelFormat::UNDEFINED;
            type        = ImageAssetType::TEXTURE_2D;
            width       = 1;
            height      = 1;
            depth       = 1;
            mipLevels   = 1;
            arrayLayers = 1;
            slices.clear();
            rawData.clear();
        }
    };

    // Runtime-only build: maps the header to an Image::Descriptor, creates the
    // matching Texture subclass and uploads every slice. Returns null on invalid
    // input (null device, version mismatch, wrong cube layer count).
    CounterPtr<Texture> CreateTextureFromAsset(Device *device, const Asset<Texture> &asset);

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::Texture> {
        using DataType                                = sky::aurora::ImageAssetData;
        static constexpr std::string_view ASSET_TYPE  = "AuroraTexture";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
