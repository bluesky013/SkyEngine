//
// Created on 2026/09/22.
//

#pragma once

#include <vegetation/VegetationTypes.h>

#include <core/template/ReferenceObject.h>
#include <framework/asset/Asset.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;
    class SerializationContext;
} // namespace sky

namespace sky::vegetation {

    // World-space density (distribution) map payload.
    struct VegetationDensityMap {
        Vector3              origin;
        float                resolution = 1.f;   // meters per sample
        uint32_t             sizeX = 0;
        uint32_t             sizeY = 0;
        std::vector<uint8_t> data;              // 0..255 density
    };

    // Backend-agnostic vegetation asset payload: biome set + palettes + density maps + placement config.
    struct VegetationAssetData {
        uint32_t                             version = 1;
        VegetationPalette                    palette;
        std::vector<VegetationDensityMap>    densityMaps;
        VegetationPlacementConfig            config;

        void Save(BinaryOutputArchive &archive) const;
        void Load(BinaryInputArchive &archive);

        static void Reflect(SerializationContext *context);
    };

    // Asset resource key type; payload lives in VegetationAssetData.
    class VegetationAsset : public RefObject {
    public:
        VegetationAsset()           = default;
        ~VegetationAsset() override = default;
    };

    using VegetationAssetPtr = std::shared_ptr<Asset<VegetationAsset>>;

    // Flat, reflected authored source used by the vegetation baker (JSON).
    struct VegetationSourceData {
        uint32_t seed                 = 0;
        float    pointsPerSquareMeter = 1.f;
        Uuid     paletteSource;
        uint32_t biomeCount           = 0;

        static void Reflect(SerializationContext *context);
    };

    // Reflects the asset payload/source and registers the vegetation asset handler.
    void RegisterVegetationAssetType();

} // namespace sky::vegetation

namespace sky {

    template <>
    struct AssetTraits<vegetation::VegetationAsset> {
        using DataType                                = vegetation::VegetationAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Vegetation";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
