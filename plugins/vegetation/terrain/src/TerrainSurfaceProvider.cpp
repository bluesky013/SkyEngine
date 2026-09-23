//
// Created on 2026/09/22.
//

#include <vegetation/terrain/TerrainSurfaceProvider.h>

#include <terrain/TerrainAddress.h>
#include <terrain/TerrainQuery.h>
#include <terrain/TerrainSystemInterface.h>

#include <core/math/Vector4.h>

#include <algorithm>

namespace sky::vegetation {

    namespace {
        constexpr float SURFACE_BOUND_Y = 1.0e6f;
    }

    TerrainSurfaceProvider::TerrainSurfaceProvider(terrain::ITerrainSystem *inSystem) : system(inSystem)
    {
        if (system != nullptr) {
            system->AddChangeListener(this);
        }
    }

    TerrainSurfaceProvider::~TerrainSurfaceProvider()
    {
        if (system != nullptr) {
            system->RemoveChangeListener(this);
        }
    }

    bool TerrainSurfaceProvider::SampleSurface(const Vector3 &worldPos, VegetationSurfaceSample &out) const
    {
        out = VegetationSurfaceSample{};
        if (system == nullptr) {
            return false;
        }

        const terrain::ITerrainField &field = system->GetField();

        float height = 0.f;
        if (!field.QueryHeight(worldPos, height)) {
            return false;
        }
        out.height = height;

        Vector3 normal;
        if (field.QueryNormal(worldPos, normal)) {
            out.normal = normal;
        }

        Vector4 weights;
        if (field.QuerySplatWeights(worldPos, weights)) {
            out.layerWeights[0] = weights.x;
            out.layerWeights[1] = weights.y;
            out.layerWeights[2] = weights.z;
            out.layerWeights[3] = weights.w;
        }

        out.valid = true;
        return true;
    }

    float TerrainSurfaceProvider::GetCellSize() const
    {
        return system != nullptr ? system->GetMeta().GetTileWorldSize() : 0.f;
    }

    void TerrainSurfaceProvider::GetCellRange(const AABB &bounds, int32_t &minX, int32_t &minY, int32_t &maxX, int32_t &maxY) const
    {
        minX = minY = maxX = maxY = 0;
        if (system == nullptr) {
            return;
        }

        terrain::TerrainTileCoord minCoord;
        terrain::TerrainTileCoord maxCoord;
        terrain::TileRangeForBounds(system->GetMeta(), bounds, minCoord, maxCoord);
        minX = minCoord.x;
        minY = minCoord.y;
        maxX = maxCoord.x;
        maxY = maxCoord.y;
    }

    AABB TerrainSurfaceProvider::GetCellBounds(int32_t cellX, int32_t cellY) const
    {
        if (system == nullptr) {
            return AABB{};
        }

        const terrain::TerrainMeta &meta = system->GetMeta();
        const Vector3 origin = terrain::TileToWorld(meta, terrain::TerrainTileCoord{cellX, cellY});
        const float size = meta.GetTileWorldSize();
        return AABB(Vector3(origin.x, -SURFACE_BOUND_Y, origin.z),
                    Vector3(origin.x + size, SURFACE_BOUND_Y, origin.z + size));
    }

    void TerrainSurfaceProvider::AddSurfaceListener(IVegetationSurfaceListener *listener)
    {
        if (listener != nullptr && std::find(listeners.begin(), listeners.end(), listener) == listeners.end()) {
            listeners.push_back(listener);
        }
    }

    void TerrainSurfaceProvider::RemoveSurfaceListener(IVegetationSurfaceListener *listener)
    {
        listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
    }

    void TerrainSurfaceProvider::OnTerrainTilesChanged(const std::vector<terrain::TerrainTileCoord> &coords)
    {
        if (system == nullptr || listeners.empty()) {
            return;
        }

        const terrain::TerrainMeta &meta = system->GetMeta();
        const float size = meta.GetTileWorldSize();

        for (const auto &coord : coords) {
            const Vector3 origin = terrain::TileToWorld(meta, coord);
            const AABB region(Vector3(origin.x, -SURFACE_BOUND_Y, origin.z),
                              Vector3(origin.x + size, SURFACE_BOUND_Y, origin.z + size));
            for (auto *listener : listeners) {
                listener->OnVegetationSurfaceChanged(region);
            }
        }
    }

} // namespace sky::vegetation
