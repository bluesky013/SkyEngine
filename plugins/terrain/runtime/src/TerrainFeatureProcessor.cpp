//
// Created on 2025/01/15.
//

#include <terrain/TerrainFeatureProcessor.h>
#include <terrain/TerrainClipmap.h>
#include <terrain/TerrainRendererCPU.h>
#include <terrain/TerrainRenderGPU.h>
#include <render/RenderScene.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    TerrainFeatureProcessor::TerrainFeatureProcessor(RenderScene *scn)
        : IFeatureProcessor(scn)
    {
    }

    TerrainFeatureProcessor::~TerrainFeatureProcessor()
    {
        Reset();
    }

    void TerrainFeatureProcessor::Tick(float /*time*/)
    {
        if (dirty) {
            if (!clipmap || !renderer) {
                BuildRenderer();
            }
            dirty = false;
        }

        // TODO: get camera position from scene view and call
        // renderer->UpdateClipmap(cameraPos);
    }

    void TerrainFeatureProcessor::Render(rdg::RenderGraph & /*rdg*/)
    {
        if (renderer) {
            renderer->Render();
        }
    }

    void TerrainFeatureProcessor::SetConfig(const ClipmapConfig &config)
    {
        clipmapConfig = config;
        dirty = true;
    }

    void TerrainFeatureProcessor::SetMaterial(const Uuid &uuid)
    {
        materialUuid = uuid;
        LoadMaterial();
        if (renderer && material) {
            renderer->SetMaterial(material);
        }
    }

    void TerrainFeatureProcessor::SetTileData(uint32_t countX, uint32_t countY,
                                               const std::vector<Uuid> &heightTiles,
                                               const std::vector<Uuid> &splatTiles,
                                               const std::vector<LayerInfo> &inLayers)
    {
        tileCountX = countX;
        tileCountY = countY;
        heightmapTileUuids = heightTiles;
        splatmapTileUuids = splatTiles;
        layers = inLayers;

        LoadTiles();
        dirty = true;
    }

    void TerrainFeatureProcessor::Reset()
    {
        renderer.reset();
        clipmap.reset();

        material = nullptr;
        heightmapTextures.clear();
        splatmapTextures.clear();
        heightmapCache.clear();

        heightmapTileUuids.clear();
        splatmapTileUuids.clear();
        layers.clear();

        tileCountX = 0;
        tileCountY = 0;
        dirty = false;
    }

    // --- CPU Query API stubs ---

    float TerrainFeatureProcessor::QueryHeight(float /*x*/, float /*z*/) const
    {
        // TODO: look up tile from world coords, bilinear interpolate heightmapCache
        return 0.f;
    }

    Vector3 TerrainFeatureProcessor::QueryNormal(float /*x*/, float /*z*/) const
    {
        // TODO: finite-difference from 4 neighboring QueryHeight samples
        return {0.f, 1.f, 0.f};
    }

    Vector4 TerrainFeatureProcessor::QuerySplatWeights(float /*x*/, float /*z*/) const
    {
        // TODO: bilinear sample splatmap tile
        return {1.f, 0.f, 0.f, 0.f};
    }

    bool TerrainFeatureProcessor::Raycast(const Ray & /*ray*/, float /*maxDist*/, TerrainHitResult & /*hit*/) const
    {
        // TODO: ray march along heightfield
        return false;
    }

    // --- Internal loading ---

    void TerrainFeatureProcessor::LoadMaterial()
    {
        auto *am = AssetManager::Get();
        auto asset = am->LoadAsset<MaterialInstance>(materialUuid);
        if (asset) {
            asset->BlockUntilLoaded();
            material = CreateMaterialInstanceFromAsset(asset);
        }
    }

    void TerrainFeatureProcessor::LoadTiles()
    {
        auto *am = AssetManager::Get();

        heightmapTextures.clear();
        for (const auto &uuid : heightmapTileUuids) {
            auto asset = am->LoadAsset<Texture>(uuid);
            if (!asset) { continue; }
            asset->BlockUntilLoaded();
            auto tex = CreateTextureFromAsset(asset);
            if (tex) {
                heightmapTextures[uuid] = static_cast<Texture2D*>(tex.Get());
            }
        }

        splatmapTextures.clear();
        for (const auto &uuid : splatmapTileUuids) {
            auto asset = am->LoadAsset<Texture>(uuid);
            if (!asset) { continue; }
            asset->BlockUntilLoaded();
            auto tex = CreateTextureFromAsset(asset);
            if (tex) {
                splatmapTextures[uuid] = static_cast<Texture2D*>(tex.Get());
            }
        }
    }

    void TerrainFeatureProcessor::BuildRenderer()
    {
        clipmap = std::make_unique<TerrainClipmap>();
        clipmap->Init(clipmapConfig);

        // Use CPU-driven renderer (Phase 1); GPU-driven is Phase 3 placeholder
        renderer = std::make_unique<TerrainRendererCPU>();
        renderer->Init(scene, clipmap.get());

        if (material) {
            renderer->SetMaterial(material);
        }

        // Bind first heightmap/splatmap tile (Phase 1: single tile; Phase 2: atlas)
        RDTexture2DPtr firstHeightmap;
        RDTexture2DPtr firstSplatmap;

        if (!heightmapTileUuids.empty()) {
            auto it = heightmapTextures.find(heightmapTileUuids[0]);
            if (it != heightmapTextures.end()) {
                firstHeightmap = it->second;
            }
        }
        if (!splatmapTileUuids.empty()) {
            auto it = splatmapTextures.find(splatmapTileUuids[0]);
            if (it != splatmapTextures.end()) {
                firstSplatmap = it->second;
            }
        }

        renderer->SetTileTextures(firstHeightmap, firstSplatmap);
    }

} // namespace sky
