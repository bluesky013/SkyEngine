//
// Created by blues on 2024/9/1.
//

#include <render/debug/DebugRenderer.h>

namespace sky {

    static std::vector<VertexAttribute> DEBUG_ATTRIBUTES = {
            VertexAttribute{VertexSemanticFlagBit::POSITION, 0, OFFSET_OF(DebugVertex, pos), rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::COLOR,    0, OFFSET_OF(DebugVertex, col),  rhi::Format::F_RGBA32},
    };

    static VertexSemanticFlags DEBUG_VTX_SEMANTICS = VertexSemanticFlagBit::POSITION | VertexSemanticFlagBit::COLOR;

    DebugRenderer::DebugRenderer()
        : geometry(new RenderGeometry())
    {
        geometry->vertexAttributes   = DEBUG_ATTRIBUTES;
        geometry->attributeSemantics = DEBUG_VTX_SEMANTICS;
    }

    void DebugRenderer::Reset()
    {
        currentColor = {1.f, 1.f, 1.f, 1.f};
        batchVertices.clear();
    }

    void DebugRenderer::SetColor(const Color &color)
    {
        currentColor = color;
    }

    void DebugRenderer::DrawLine(const Line &line)
    {
        DrawLine(line.begin, line.end);
    }

    void DebugRenderer::DrawLine(const Vector3 &from, const Vector3 &to)
    {
        DebugVertex begin = {
            Vector4{from.x, from.y, from.z, 1.f},
            currentColor
        };

        DebugVertex end = {
            Vector4{to.x, to.y, to.z, 1.f},
            currentColor
        };

        batchVertices.emplace_back(begin);
        batchVertices.emplace_back(end);
    }

    void DebugRenderer::DrawSphere(const Sphere &sphere)
    {
        static const uint32_t STACK_COUNT = 16;
        static const uint32_t SECTOR_COUNT = 32;

        float sectorStep  = 2 * PI / SECTOR_COUNT;
        float stackStep   = PI / STACK_COUNT;


        for(int i = 0; i < STACK_COUNT; ++i)                // starting from pi/2 to -pi/2
        {
            float stackAngle1 = PI / 2 - static_cast<float>(i) * stackStep;
            float stackAngle2 = PI / 2 - static_cast<float>(i + 1) * stackStep;

            float xz1 = sphere.radius * cosf(stackAngle1);
            float y1  = sphere.radius * sinf(stackAngle1);

            float xz2 = sphere.radius * cosf(stackAngle2);
            float y2  = sphere.radius * sinf(stackAngle2);

            for(int j = 0; j < SECTOR_COUNT; ++j)           // starting from 0 to 2pi
            {
                float sectorAngle1 = static_cast<float>(j) * sectorStep;
                float sectorAngle2 = static_cast<float>(j + 1) * sectorStep;

                // vertex position (x, y, z)
                float x1 = xz1 * cosf(sectorAngle1);
                float z1 = xz1 * sinf(sectorAngle1);

                float x2 = xz2 * cosf(sectorAngle1);
                float z2 = xz2 * sinf(sectorAngle1);

                DrawLine(Vector3{x1, y1, z1}, Vector3{x2, y2, z2});

                if (i != 0) {
                    float x3 = xz1 * cosf(sectorAngle2);
                    float z3 = xz1 * sinf(sectorAngle2);
                    DrawLine(Vector3{x1, y1, z1}, Vector3{x3, y1, z3});
                }
            }
        }
    }

    void DebugRenderer::DrawAABB(const AABB &aabb)
    {
        DrawLine(Vector3(aabb.min[0], aabb.min[1], aabb.min[2]), Vector3(aabb.max[0], aabb.min[1], aabb.min[2]));
        DrawLine(Vector3(aabb.max[0], aabb.min[1], aabb.min[2]), Vector3(aabb.max[0], aabb.max[1], aabb.min[2]));
        DrawLine(Vector3(aabb.max[0], aabb.max[1], aabb.min[2]), Vector3(aabb.min[0], aabb.max[1], aabb.min[2]));
        DrawLine(Vector3(aabb.min[0], aabb.max[1], aabb.min[2]), Vector3(aabb.min[0], aabb.min[1], aabb.min[2]));
        DrawLine(Vector3(aabb.min[0], aabb.min[1], aabb.min[2]), Vector3(aabb.min[0], aabb.min[1], aabb.max[2]));
        DrawLine(Vector3(aabb.max[0], aabb.min[1], aabb.min[2]), Vector3(aabb.max[0], aabb.min[1], aabb.max[2]));
        DrawLine(Vector3(aabb.max[0], aabb.max[1], aabb.min[2]), Vector3(aabb.max[0], aabb.max[1], aabb.max[2]));
        DrawLine(Vector3(aabb.min[0], aabb.max[1], aabb.min[2]), Vector3(aabb.min[0], aabb.max[1], aabb.max[2]));
        DrawLine(Vector3(aabb.min[0], aabb.min[1], aabb.max[2]), Vector3(aabb.max[0], aabb.min[1], aabb.max[2]));
        DrawLine(Vector3(aabb.max[0], aabb.min[1], aabb.max[2]), Vector3(aabb.max[0], aabb.max[1], aabb.max[2]));
        DrawLine(Vector3(aabb.max[0], aabb.max[1], aabb.max[2]), Vector3(aabb.min[0], aabb.max[1], aabb.max[2]));
        DrawLine(Vector3(aabb.min[0], aabb.max[1], aabb.max[2]), Vector3(aabb.min[0], aabb.min[1], aabb.max[2]));
    }

    void DebugRenderer::Render()
    {
        auto vtxSize = static_cast<uint32_t>(batchVertices.size() * sizeof(DebugVertex));

        if (!vertexBuffer || vtxSize > capacity) {
            capacity = std::max(capacity * 2, vtxSize);

            vertexBuffer = new Buffer();
            vertexBuffer->Init(capacity, rhi::BufferUsageFlagBit::VERTEX, rhi::MemoryType::CPU_TO_GPU);

            geometry->vertexBuffers.clear();
            geometry->vertexBuffers.emplace_back(VertexBuffer {
                vertexBuffer, 0, capacity, static_cast<uint32_t>(sizeof(DebugVertex))
            });
            geometry->version++;
        }

        linear.vertexCount = static_cast<uint32_t>(batchVertices.size());
    }

} // namespace sky