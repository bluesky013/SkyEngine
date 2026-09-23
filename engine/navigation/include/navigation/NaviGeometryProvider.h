//
// Created on 2026/09/22.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>

#include <cstdint>

namespace sky::ai {

    // Receives world-space geometry for nav mesh generation from a provider.
    class INaviGeometrySink {
    public:
        virtual ~INaviGeometrySink() = default;

        virtual void AddTriangles(const Vector3 *vertices, uint32_t vertexCount,
                                  const uint32_t *indices, uint32_t indexCount) = 0;
    };

    // A source of nav mesh geometry (e.g. terrain). Implemented outside the navigation core so the
    // core stays free of terrain/render/physics types.
    class INaviGeometryProvider {
    public:
        virtual ~INaviGeometryProvider() = default;

        // Collects world-space triangles over bounds. Returns false if coverage was incomplete.
        virtual bool Collect(const AABB &bounds, INaviGeometrySink &sink) = 0;
    };

} // namespace sky::ai
