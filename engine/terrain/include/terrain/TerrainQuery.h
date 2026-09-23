//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>

#include <core/math/Vector3.h>
#include <core/math/Vector4.h>

#include <cstdint>

namespace sky::terrain {

    struct TerrainRaycastHit {
        Vector3 position;
        Vector3 normal;
        float   distance = 0.f;
    };

    // Engine-side terrain query interface (implementation lives in a plugin).
    class ITerrainField {
    public:
        virtual ~ITerrainField() = default;

        virtual const TerrainMeta &GetMeta() const = 0;
        virtual uint32_t GetLoadedTileCount() const = 0;

        virtual bool GetTileHeights(const TerrainTileCoord &coord, const float *&outHeights, uint32_t &outVertexSize) const = 0;

        virtual bool QueryHeight(const Vector3 &worldPos, float &outHeight) const = 0;
        virtual bool QueryNormal(const Vector3 &worldPos, Vector3 &outNormal) const = 0;
        virtual bool QuerySplatWeights(const Vector3 &worldPos, Vector4 &outWeights) const = 0;
        virtual bool Raycast(const Vector3 &origin, const Vector3 &dir, float maxDist, TerrainRaycastHit &out) const = 0;
    };

} // namespace sky::terrain
