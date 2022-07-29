//
// Created by Zach Lee on 2022/7/19.
//

#pragma once

#include <render/RenderMesh.h>
#include <render/resources/Mesh.h>

namespace sky {

    class StaticMesh : public RenderMesh {
    public:
        ~StaticMesh() = default;

        void SetMesh(RDMeshPtr);

    private:
        friend class StaticMeshFeature;
        StaticMesh() = default;
        RDMeshPtr mesh;

        drv::VertexInputPtr standard;
        drv::VertexInputPtr positionOnly;
    };

}