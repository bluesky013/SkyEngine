//
// Created by Zach Lee on 2022/7/19.
//

#pragma once

#include <core/util/Macros.h>
#include <render/RenderMesh.h>
#include <render/resources/Mesh.h>
#include <vulkan/VertexAssembly.h>

namespace sky {

    class StaticMesh : public RenderMesh {
    public:
        ~StaticMesh() = default;

        SKY_DISABLE_COPY(StaticMesh)

        void SetMesh(RDMeshPtr);

    private:
        void Setup();

        friend class StaticMeshFeature;
        StaticMesh() = default;
        RDMeshPtr mesh;
        drv::VertexAssemblyPtr vertexAssembly;

        using RenderPrimitivePtr = std::unique_ptr<RenderPrimitive>;
        std::vector<RenderPrimitivePtr> primitives;
    };

}