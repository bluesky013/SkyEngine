//
// Created by Zach Lee on 2022/7/31.
//

#include <core/math/MathUtil.h>
#include <render/RenderMesh.h>
#include <render/RenderScene.h>

namespace sky {

    void RenderMesh::SetWorldMatrix(const Matrix4 &matrix)
    {
        objectInfo.worldMatrix            = matrix;
        objectInfo.inverseTransposeMatrix = glm::inverseTranspose(matrix);
        UpdateBuffer();
    }

    void RenderMesh::UpdateBuffer()
    {
        objectBuffer->Write(objectInfo);
    }

    void RenderMesh::AddToScene(RenderScene &scene)
    {
        auto objPool    = scene.GetObjectSetPool();
        auto bufferPool = scene.GetObjectBufferPool();
        objectSet       = objPool->Allocate();
        objectBuffer    = bufferPool->Allocate();

        Buffer::Descriptor bufferDesc = {};
        bufferDesc.size               = sizeof(ObjectInfo);
        bufferDesc.memory             = VMA_MEMORY_USAGE_CPU_TO_GPU;
        bufferDesc.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferDesc.allocCPU           = true;
        auto buffer                   = std::make_shared<Buffer>(bufferDesc);
        buffer->InitRHI();

        UpdateBuffer();

        objectSet->UpdateBuffer(0, objectBuffer);
        objectSet->Update();
    }

    void RenderMesh::RemoveFromScene(RenderScene &scene)
    {
        auto bufferPool = scene.GetObjectBufferPool();
        bufferPool->Free(objectBuffer->GetID());
        objectBuffer = nullptr;
    }

    void RenderMesh::OnRender(RenderScene &scene)
    {
        objectBuffer->RequestUpdate();
    }
} // namespace sky