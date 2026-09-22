//
// Created on 2026/09/22.
//

#pragma once

#include <core/math/Vector3.h>

#include <vector>

namespace sky::phy {

    // Plain debug vertex (no render resource types).
    struct PhysicsDebugVertex {
        Vector3 position;
        float   color[4] = {1.f, 1.f, 1.f, 1.f};
    };

    // Render-agnostic physics debug geometry: lines (2 verts each) and triangles (3 verts each).
    struct PhysicsDebugGeometry {
        std::vector<PhysicsDebugVertex> lines;
        std::vector<PhysicsDebugVertex> triangles;

        void AddLine(const Vector3 &a, const Vector3 &b, const float color[4]);
        void AddTriangle(const Vector3 &a, const Vector3 &b, const Vector3 &c, const float color[4]);

        void Clear()
        {
            lines.clear();
            triangles.clear();
        }

        bool IsEmpty() const { return lines.empty() && triangles.empty(); }
    };

} // namespace sky::phy
