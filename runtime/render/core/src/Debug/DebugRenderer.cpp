//
// Created by blues on 2024/9/1.
//

#include <render/debug/DebugRenderer.h>
#include <render/RenderPrimitive.h>

namespace sky {

    static std::vector<VertexAttribute> DEBUG_ATTRIBUTES = {
            VertexAttribute{VertexSemanticFlagBit::POSITION, 0, OFFSET_OF(DebugVertex, pos), rhi::Format::F_RGB32},
            VertexAttribute{VertexSemanticFlagBit::COLOR,    0, OFFSET_OF(DebugVertex, col),  rhi::Format::F_RGBA8},
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
        batches.clear();

        currentColor = {255, 255, 255, 255};
        currentKey = DebugBatchKey{};
        batchVertices = &batches[currentKey];
        lastKey = currentKey;
    }

    void DebugRenderer::SetColor(const Color32 &color)
    {
        currentColor = color;
    }

    void DebugRenderer::SetDepthWrite(bool enable)
    {
        currentKey.depthWrite = enable;
    }

    void DebugRenderer::SetDepthTest(bool enable)
    {
        currentKey.depthTest = enable;
    }

    void DebugRenderer::SetBlendEnable(bool enable)
    {
        currentKey.enableBlend = enable;
    }

    void DebugRenderer::SetTopo(rhi::PrimitiveTopology topo)
    {
        currentKey.topology = topo;
    }

    void DebugRenderer::ResetBatch()
    {
        if (!std::equal_to<DebugBatchKey>{}(currentKey, lastKey)) {
            batchVertices = &batches[currentKey];
        }

        lastKey = currentKey;
    }

    void DebugRenderer::DrawLine(const Line &line)
    {
        DrawLine(line.begin, line.end);
    }

    void DebugRenderer:: DrawTriangle(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3)
    {
        ResetBatch();

        batchVertices->emplace_back(DebugVertex{v1, currentColor});
        batchVertices->emplace_back(DebugVertex{v2, currentColor});
        batchVertices->emplace_back(DebugVertex{v3, currentColor});
    }

    void DebugRenderer::DrawLine(const Vector3 &from, const Vector3 &to)
    {
        ResetBatch();

        batchVertices->emplace_back(DebugVertex{from, currentColor});
        batchVertices->emplace_back(DebugVertex{to, currentColor});
    }

    void DebugRenderer::DrawSphere(const Sphere &sphere)
    {
        ResetBatch();

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

    void DebugRenderer::Render(std::vector<RenderPrimitive*> &primitives)
    {
        uint32_t vtxSize = 0;
        for (auto &[key, batch] : batches) {
            vtxSize += static_cast<uint32_t>(batch.size()) * sizeof(DebugVertex);
        }

        if (vtxSize == 0) {
            return;
        }

        if (!vertexBuffer || vtxSize > capacity) {
            capacity = std::max(capacity * 2, vtxSize);

            vertexBuffer = new DynamicBuffer();
            vertexBuffer->Init(capacity, rhi::BufferUsageFlagBit::VERTEX);

            geometry->vertexBuffers.clear();
            geometry->vertexBuffers.emplace_back(VertexBuffer {
                    vertexBuffer, 0, capacity, static_cast<uint32_t>(sizeof(DebugVertex))
            });
            geometry->version++;
        }
        vertexBuffer->SwapBuffer();
        SKY_ASSERT(batches.size() <= primitives.size());

        uint32_t firstVertex = 0;
        uint32_t batchIndex = 0;
        for (auto &[key, batch] : batches) {
            auto count = static_cast<uint32_t>(batch.size());
            if (count == 0) {
                continue;
            }
            auto *primitive = primitives[batchIndex++];
            primitive->batches[0].topo = key.topology;

            primitive->geometry = geometry;
            primitive->args.clear();

            uint32_t dataOffset = firstVertex * sizeof(DebugVertex);
            uint32_t dataSize   = count * sizeof(DebugVertex);
            vertexBuffer->Update(reinterpret_cast<uint8_t *>(batch.data()), dataOffset, dataSize);

            rhi::CmdDrawLinear linear = {};
            linear.firstVertex = firstVertex;
            linear.vertexCount = count;
            primitive->args.emplace_back(linear);

            firstVertex += count;
        }
    }

    void DebugRenderer::Render(RenderPrimitive *primitive)
    {
        std::vector<RenderPrimitive*> primitives;
        primitives.emplace_back(primitive);

        Render(primitives);
    }

} // namespace sky