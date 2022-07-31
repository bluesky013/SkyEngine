//
// Created by Zach Lee on 2022/7/31.
//

#include <render/RenderMesh.h>
#include <render/RenderScene.h>

namespace sky {

    void RenderMesh::SetWorldMatrix(const Matrix4& matrix)
    {
        objectInfo.worldMatrix = matrix;
        objectInfo.inverseTransposeMatrix = glm::inverseTranspose(matrix);

        UpdateBuffer();
    }

    void RenderMesh::UpdateBuffer()
    {
        objectBuffer->Write(&objectInfo);
    }

    void RenderMesh::AddToScene(RenderScene& scene)
    {
        auto objPool = scene.GetObjectSetPool();
        objectSet = objPool->Allocate();

        Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = sizeof(ObjectInfo);
        bufferDesc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
        bufferDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferDesc.allocCPU = true;
        auto buffer = std::make_shared<Buffer>(bufferDesc);
        buffer->InitRHI();

        objectBuffer = std::make_shared<BufferView>(buffer, bufferDesc.size, 0);

        objectSet->UpdateBuffer(0, objectBuffer);
        objectSet->Update();
    }

    void RenderMesh::RemoveFromScene(RenderScene& scene)
    {
    }

    void RenderMesh::OnRender(RenderScene& scene)
    {
        objectBuffer->RequestUpdate();
    }
}