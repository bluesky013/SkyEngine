//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>

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

namespace sky::terrain {

    // One LOD of a tile: height samples in the declared format plus RGBA8 splat tiles (near LODs only).
    struct TerrainLodPayload {
        std::vector<uint8_t>              height;
        std::vector<std::vector<uint8_t>> splat;
    };

    // Per-tile payload holding the LOD chain; lods[0] is the highest detail.
    struct TerrainTilePayload {
        TerrainTileCoord               coord;
        std::vector<TerrainLodPayload> lods;

        const TerrainLodPayload *GetLod(uint32_t lod) const
        {
            return lod < lods.size() ? &lods[lod] : nullptr;
        }
        uint32_t GetLodCount() const { return static_cast<uint32_t>(lods.size()); }
    };

    // Backend-agnostic terrain asset payload: layout metadata + tile manifest + addressable tile LOD chains.
    struct TerrainAssetData {
        uint32_t                        version = 1;
        TerrainMeta                     meta;
        TerrainTileManifest             manifest;
        std::vector<TerrainTilePayload> tiles;

        void Save(BinaryOutputArchive &archive) const;
        void Load(BinaryInputArchive &archive);

        static void Reflect(SerializationContext *context);
    };

    // Asset resource key type; the payload lives in TerrainAssetData.
    class TerrainAsset : public RefObject {
    public:
        TerrainAsset() = default;
        ~TerrainAsset() override = default;
    };

    using TerrainAssetPtr = std::shared_ptr<Asset<TerrainAsset>>;

    // Reflects TerrainAssetData and registers the terrain asset handler.
    void RegisterTerrainAssetType();

} // namespace sky::terrain

namespace sky {

    template <>
    struct AssetTraits<terrain::TerrainAsset> {
        using DataType                                = terrain::TerrainAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Terrain";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
