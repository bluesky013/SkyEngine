//
// Created on 2026/09/22.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>

#include <cstdint>

namespace sky::vegetation {

    class IVegetationSurfaceListener;

    // A sampled world surface point consumed by vegetation placement.
    struct VegetationSurfaceSample {
        float   height       = 0.f;
        Vector3 normal       = VEC3_Y;
        float   layerWeights[4] = {0.f, 0.f, 0.f, 0.f};
        bool    valid        = false;
    };

    // Abstract world surface used by vegetation placement/streaming (terrain is one implementation).
    class IVegetationSurfaceProvider {
    public:
        virtual ~IVegetationSurfaceProvider() = default;

        virtual bool  SampleSurface(const Vector3 &worldPos, VegetationSurfaceSample &out) const = 0;

        virtual float GetCellSize() const = 0;

        // Inclusive cell range covering a world-space bounds (X/Z extent).
        virtual void  GetCellRange(const AABB &bounds, int32_t &minX, int32_t &minY, int32_t &maxX, int32_t &maxY) const = 0;

        virtual AABB  GetCellBounds(int32_t cellX, int32_t cellY) const = 0;

        // Surface-change notification. Default no-op so simple providers need not implement it.
        virtual void AddSurfaceListener(IVegetationSurfaceListener *listener) {}
        virtual void RemoveSurfaceListener(IVegetationSurfaceListener *listener) {}
    };

    // Notified when surface data changes so overlapping vegetation cells can be invalidated.
    class IVegetationSurfaceListener {
    public:
        virtual ~IVegetationSurfaceListener() = default;

        virtual void OnVegetationSurfaceChanged(const AABB &region) = 0;
    };

} // namespace sky::vegetation
