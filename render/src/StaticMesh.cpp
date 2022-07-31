//
// Created by Zach Lee on 2022/7/19.
//

#include <render/StaticMesh.h>
#include <render/RenderConstants.h>

namespace sky {

    void StaticMesh::SetMesh(RDMeshPtr m)
    {
        mesh = m;
        auto& subMeshes = m->GetSubMeshes();
        for (auto& subMesh : subMeshes) {
            auto primitive = std::make_shared<RenderPrimitive>();
            primitive->SetViewMask(MAIN_CAMERA_TAG);
            primitive->SetAABB(subMesh.aabb);
            primitive->SetMaterial(subMesh.material);
        }

    }

    void StaticMesh::Setup()
    {

    }

}