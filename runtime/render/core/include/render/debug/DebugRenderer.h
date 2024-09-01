//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/math/Color.h>
#include <core/shapes/Shapes.h>
#include <vector>

namespace sky {

    struct DebugVertex {
        Vector4 position;
        Color color;
    };

    class DebugRenderer {
    public:
        DebugRenderer() = default;
        ~DebugRenderer() = default;

        void Reset();
        void SetColor(const Color &color);

        void DrawLine(const Line &line);
        void DrawLine(const Vector3 &from, const Vector3 &to);

        void DrawSphere(const Sphere &sphere);
        void DrawAABB(const AABB &aabb);
    private:
        Color currentColor = {1.f, 1.f, 1.f, 1.f};

        std::vector<DebugVertex> batchVertices;
        std::vector<uint32_t> batchIndices;
    };

} // namespace sky
