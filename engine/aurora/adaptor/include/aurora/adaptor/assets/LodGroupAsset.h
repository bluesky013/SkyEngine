//
// Aurora lod group asset: versioned CPU payload of ordered LOD levels, each
// binding a screen-size threshold to a mesh asset (Uuid). The device-available
// build path resolves those Uuids into aurora::LodGroup levels.
//

#pragma once

#include <aurora/resource/LodGroup.h>
#include <core/util/Uuid.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace sky::aurora {

    struct LodGroupLevelData {
        float screenSize = 1.f;
        Uuid  mesh;
    };

    struct LodGroupAssetData {
        static constexpr uint32_t CURRENT_VERSION = 1;

        uint32_t                       version = CURRENT_VERSION;
        std::vector<LodGroupLevelData> levels;

        void Save(BinaryOutputArchive &ar) const;
        void Load(BinaryInputArchive &ar);

        void clear();
    };

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::LodGroup> {
        using DataType                                = sky::aurora::LodGroupAssetData;
        static constexpr std::string_view ASSET_TYPE  = "AuroraLodGroup";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
