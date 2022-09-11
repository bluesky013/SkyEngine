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
        objectInfo.inverseTransposeMatrix = matrix.InverseTranspose();
        UpdateBuffer();
    }

    const ObjectInfo& RenderMesh::GetObjectInfo() const
    {
        return objectInfo;
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