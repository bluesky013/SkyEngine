//
// Created by blues on 2023/12/17.
//

#include <render/mesh/GridRenderer.h>
#include <render/RHI.h>
#include <core/math/Vector4.h>
#include <rhi/Stream.h>
namespace sky {

    GridRenderer &GridRenderer::SetUp(const Desc &desc)
    {
        uint32_t vertexSize = 4 * sizeof(Vector4);
        uint32_t indexSize = 6 * sizeof(uint32_t);

        auto ext = static_cast<float>(desc.ext);

        auto *ptr = malloc(UCast(VertexSemantic::MAX) * sizeof(Vector4) * 4);

        auto *pos = static_cast<Vector4*>(ptr);
        pos[0] = Vector4(-ext, 0.f, -ext, 1.f);
        pos[1] = Vector4( ext, 0.f, -ext, 1.f);
        pos[2] = Vector4( ext, 0.f,  ext, 1.f);
        pos[3] = Vector4(-ext, 0.f,  ext, 1.f);

        Vector4 *uv = pos + 4;
        uv[0] = Vector4(0.f, 0.f, 0.f, 0.f) * ext;
        uv[1] = Vector4(1.f, 0.f, 0.f, 0.f) * ext;
        uv[2] = Vector4(1.f, 1.f, 0.f, 0.f) * ext;
        uv[3] = Vector4(0.f, 1.f, 0.f, 0.f) * ext;

        Vector4 *normal = uv + 4;
        normal[0] = Vector4(0.f, 1.f, 0.f, 1.f);
        normal[1] = Vector4(0.f, 1.f, 0.f, 1.f);
        normal[2] = Vector4(0.f, 1.f, 0.f, 1.f);
        normal[3] = Vector4(0.f, 1.f, 0.f, 1.f);

        Vector4 *tangent = normal + 4;
        tangent[0] = Vector4(1.f, 0.f, 0.f, 1.f);
        tangent[1] = Vector4(1.f, 0.f, 0.f, 1.f);
        tangent[2] = Vector4(1.f, 0.f, 0.f, 1.f);
        tangent[3] = Vector4(1.f, 0.f, 0.f, 1.f);

        Vector4 *color = tangent + 4;
        color[0] = Vector4(1.f, 1.f, 1.f, 1.f);
        color[1] = Vector4(1.f, 1.f, 1.f, 1.f);
        color[2] = Vector4(1.f, 1.f, 1.f, 1.f);
        color[3] = Vector4(1.f, 1.f, 1.f, 1.f);

        void *iPtr = malloc(indexSize);
        auto *indices = static_cast<uint32_t *>(iPtr);
        indices[0] = 2;
        indices[1] = 1;
        indices[2] = 0;
        indices[3] = 0;
        indices[4] = 3;
        indices[5] = 2;


        auto *device = RHI::Get()->GetDevice();
        auto *queue = device->GetQueue(rhi::QueueType::TRANSFER);
        rhi::BufferUploadRequest request = {};
        request.size = 4 * sizeof(Vector4);
        request.offset = 0;

        vertexBuffers.resize(UCast(VertexSemantic::MAX));
        for (uint32_t i = 0; i < UCast(VertexSemantic::MAX); ++i) {
            vertexBuffers[i] = std::make_shared<Buffer>();
            vertexBuffers[i]->Init(vertexSize, rhi::BufferUsageFlagBit::TRANSFER_DST | rhi::BufferUsageFlagBit::VERTEX, rhi::MemoryType::GPU_ONLY);
            request.source = std::make_shared<rhi::RawPtrStream>(reinterpret_cast<const uint8_t*>(&pos[i * 4]));
            queue->UploadBuffer(vertexBuffers[i]->GetRHIBuffer(), request);
        }

        indexBuffer = std::make_shared<Buffer>();
        indexBuffer->Init(indexSize, rhi::BufferUsageFlagBit::TRANSFER_DST | rhi::BufferUsageFlagBit::INDEX, rhi::MemoryType::GPU_ONLY);
        request.size = indexSize;
        request.source = std::make_shared<rhi::RawPtrStream>(reinterpret_cast<const uint8_t*>(iPtr));
        queue->UploadBuffer(indexBuffer->GetRHIBuffer(), request);
        auto handle = queue->CreateTask([ptr, iPtr]() {
            free(ptr);
            free(iPtr);
        });
        queue->Wait(handle);
        return *this;
    }

    RDMeshPtr GridRenderer::BuildMesh(const RDMaterialInstancePtr &mat)
    {
        auto mesh = std::make_shared<Mesh>();
        mesh->AddSubMesh({0, 4, 0, 6, mat, AABB{}});
        for (auto &vb : vertexBuffers) {
            mesh->AddVertexBuffer(vb);
        }
        mesh->SetIndexBuffer(indexBuffer);
        mesh->SetIndexType(rhi::IndexType::U32);

        return mesh;
    }

} // namespace sky