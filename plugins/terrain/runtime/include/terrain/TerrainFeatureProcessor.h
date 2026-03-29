//
// Created on 2025/01/15.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/resource/Material.h>
#include <render/resource/Texture.h>
#include <terrain/TerrainBase.h>
#include <core/util/Uuid.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/shapes/Base.h>
#include <unordered_map>

namespace sky {

    class ITerrainRenderer;
    class TerrainClipmap;

    struct TerrainHitResult {
        Vector3 position;
        Vector3 normal;
        float   distance = 0.f;
    };

    class TerrainFeatureProcessor : public IFeatureProcessor {
    public:
        explicit TerrainFeatureProcessor(RenderScene *scn);
        ~TerrainFeatureProcessor() override;

        void Tick(float time) override;
        void Render(rdg::RenderGraph &rdg) override;

        // --- Configuration (called by TerrainComponent) ---
        void SetConfig(const ClipmapConfig &config);
        void SetMaterial(const Uuid &materialUuid);
        void SetTileData(uint32_t tileCountX, uint32_t tileCountY,
                         const std::vector<Uuid> &heightmapTiles,
                         const std::vector<Uuid> &splatmapTiles,
                         const std::vector<LayerInfo> &layers);
        void Reset();

        // --- CPU Query API ---
        float   QueryHeight(float x, float z) const;
        Vector3 QueryNormal(float x, float z) const;
        Vector4 QuerySplatWeights(float x, float z) const;
        bool    Raycast(const Ray &ray, float maxDist, TerrainHitResult &hit) const;

        // --- GPU Resource Handles (Phase 2 stubs) ---
        RDTexture2DPtr GetHeightmapAtlas() const { return nullptr; }
        RDTexture2DPtr GetSplatmapAtlas() const { return nullptr; }
        const ClipmapConfig &GetClipmapConfig() const { return clipmapConfig; }

    private:
        void LoadMaterial();
        void LoadTiles();
        void BuildRenderer();

        ClipmapConfig clipmapConfig;

        // Material
        Uuid materialUuid;
        RDMaterialInstancePtr material;

        // Tile data
        uint32_t tileCountX = 0;
        uint32_t tileCountY = 0;
        std::vector<Uuid> heightmapTileUuids;
        std::vector<Uuid> splatmapTileUuids;
        std::vector<LayerInfo> layers;

        // GPU tile textures
        std::unordered_map<Uuid, RDTexture2DPtr> heightmapTextures;
        std::unordered_map<Uuid, RDTexture2DPtr> splatmapTextures;

        // CPU tile data (for queries)
        struct TileCache {
            uint32_t width  = 0;
            uint32_t height = 0;
            std::vector<uint16_t> data; // R16 data
        };
        std::unordered_map<Uuid, TileCache> heightmapCache;

        // Clipmap + Renderer
        std::unique_ptr<TerrainClipmap>    clipmap;
        std::unique_ptr<ITerrainRenderer>  renderer;

        bool dirty = false;
    };

} // namespace sky
