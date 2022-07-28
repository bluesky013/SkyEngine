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

    private:
        friend class StaticMeshFeature;
        StaticMesh() = default;
        RDMeshPtr mesh;
    };

}