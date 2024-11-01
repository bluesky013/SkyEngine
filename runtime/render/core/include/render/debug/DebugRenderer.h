//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/math/Color.h>
#include <core/shapes/Shapes.h>
#include <core/hash/Crc32.h>
#include <vector>
#include <render/RenderGeometry.h>

namespace sky {
    struct RenderPrimitive;

    struct DebugVertex {
        Vector4 pos;
        Color32 col;
    };

    struct DebugBatchKey {
        bool depthTest   = true;
        bool depthWrite  = false;
        bool enableBlend = false;
        bool padding     = false;
        rhi::PrimitiveTopology topology = rhi::PrimitiveTopology::LINE_LIST;
    };
} // namespace sky

namespace std {

    template <>
    struct hash<sky::DebugBatchKey> {
        size_t operator()(const sky::DebugBatchKey &flags) const noexcept
        {
            return static_cast<size_t>(sky::Crc32::Cal(flags));
        }
    };

    template <>
    struct equal_to<sky::DebugBatchKey> {
        bool operator()(const sky::DebugBatchKey &x, const sky::DebugBatchKey &y) const noexcept
        {
            return x.depthTest == y.depthTest &&
                x.depthWrite == y.depthWrite &&
                x.enableBlend == y.enableBlend &&
                x.topology == y.topology;
        }
    };

} // namespace std

namespace sky {
    class DebugRenderer {
    public:
        DebugRenderer();
        ~DebugRenderer() = default;

        void Reset();
        void SetColor(const Color32 &color);

        void SetDepthWrite(bool enable);
        void SetDepthTest(bool enable);
        void SetBlendEnable(bool enable);
        void SetTopo(rhi::PrimitiveTopology topo);

        void DrawLine(const Line &line);
        void DrawLine(const Vector3 &from, const Vector3 &to);

        void DrawSphere(const Sphere &sphere);
        void DrawAABB(const AABB &aabb);

        uint32_t GetBatchSize() const { return static_cast<uint32_t>(batches.size()); }

        void Render(RenderPrimitive *primitive);
        void Render(std::vector<RenderPrimitive*> primitives);
    private:
        void ResetBatch();

        DebugBatchKey currentKey;
        Color32 currentColor = {255, 255, 255, 255};

        using DebugBatch = std::vector<DebugVertex>;

        RDDynamicBuffer vertexBuffer;
        RenderGeometryPtr geometry;

        DebugBatch *batchVertices = nullptr;
        std::unordered_map<DebugBatchKey, DebugBatch> batches;

        uint32_t capacity = 0;
    };

} // namespace sky
