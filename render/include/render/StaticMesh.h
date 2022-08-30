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

        void OnRender(RenderScene &scene) override;

        void SetMesh(const RDMeshPtr &mesh);

        void OnGatherRenderPrimitives(RenderView &view) override;

    private:
        friend class StaticMeshFeature;
        StaticMesh() = default;
        RDMeshPtr              mesh;
        drv::VertexAssemblyPtr vertexAssembly;
        drv::VertexInputPtr    vertexInput;

        using RenderPrimitivePtr = std::unique_ptr<RenderMeshPrimitive>;
        std::vector<RenderPrimitivePtr> primitives;
    };

} // namespace sky