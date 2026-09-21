//
// Created on 2026/09/21.
//

#pragma once

#include <navigation/NaviMesh.h>

#include <framework/asset/Asset.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;
    class SerializationContext;
} // namespace sky

namespace sky::ai {

    enum class NaviMeshExportMode : uint32_t {
        Tiled = 0,
        Full
    };

    struct NaviMeshBuildParams {
        NaviAgentConfig    agent;
        NaviMeshResolution resolution;
        AABB               bounds;
        float              maxSimplificationError = 1.3f;
        int32_t            borderSize = 0;
        uint32_t           version = 1;
    };

    struct NaviMeshTilePayload {
        int32_t              tx    = 0;
        int32_t              ty    = 0;
        uint32_t             layer = 0;
        std::vector<uint8_t> data;
    };

    // Backend-agnostic nav mesh asset payload. Tiled mode carries addressable per-tile blobs plus build
    // params so a pager can load a subset; Full mode carries one monolithic blob.
    struct NaviMeshData {
        NaviMeshExportMode               mode = NaviMeshExportMode::Tiled;
        NaviMeshBuildParams              params;
        std::vector<NaviMeshTilePayload> tiles;
        std::vector<uint8_t>             fullData;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;

        static void Reflect(SerializationContext *context);
    };

    using NaviMeshAssetPtr = std::shared_ptr<Asset<NaviMesh>>;

    CounterPtr<NaviMesh> CreateNaviMeshFromAsset(const NaviMeshAssetPtr &asset);

} // namespace sky::ai

namespace sky {

    template <>
    struct AssetTraits<ai::NaviMesh> {
        using DataType                                = ai::NaviMeshData;
        static constexpr std::string_view ASSET_TYPE  = "NaviMesh";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
