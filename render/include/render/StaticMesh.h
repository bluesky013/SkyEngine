//
// Created by Zach Lee on 2022/7/19.
//

#pragma once

#include <render/RenderMesh.h>
#include <render/resources/Mesh.h>
#include <render/resources/DescirptorGroup.h>

namespace sky {

    class StaticMesh : public RenderMesh {
    public:
        ~StaticMesh() = default;

        void SetMesh(RDMeshPtr);

    private:
        void Setup();

        friend class StaticMeshFeature;
        StaticMesh() = default;
        RDMeshPtr mesh;
        RDDesGroupPtr objectSet;

        drv::VertexInputPtr standard;
        drv::VertexInputPtr positionOnly;

        // key : renderOption
        std::unordered_map<uint32_t, drv::GraphicsPipelinePtr> cachedPso;
    };

}