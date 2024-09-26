//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/math/Color.h>
#include <core/shapes/Shapes.h>
#include <vector>
#include <render/RenderGeometry.h>

namespace sky {
    struct RenderPrimitive;

    struct DebugVertex {
        Vector4 pos;
        Color32 col;
    };

    class DebugRenderer {
    public:
        DebugRenderer();
        ~DebugRenderer() = default;

        void Reset();
        void SetColor(const Color32 &color);

        void DrawLine(const Line &line);
        void DrawLine(const Vector3 &from, const Vector3 &to);

        void DrawSphere(const Sphere &sphere);
        void DrawAABB(const AABB &aabb);

        void Render(RenderPrimitive *primitve);
    private:
        Color32 currentColor = {255, 255, 255, 255};

        RDDynamicBuffer vertexBuffer;
        RenderGeometryPtr geometry;
        std::vector<DebugVertex> batchVertices;

        uint32_t capacity = 0;
    };

} // namespace sky
