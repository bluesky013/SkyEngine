//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/RenderPrimitive.h>
#include <core/math/Matrix4.h>

namespace sky {
    class RenderScene;

    class StaticMeshRenderer {
    public:
        StaticMeshRenderer() = default;
        ~StaticMeshRenderer();

        void AttachScene(RenderScene *scn);
        void SetMesh(const RDMeshPtr &mesh);
        void UpdateTransform(const Matrix4 &matrix);

    private:
        RenderScene *scene = nullptr;

        RDMeshPtr mesh;
        std::list<std::unique_ptr<RenderPrimitive>> primitives;
        rhi::VertexAssemblyPtr va;
        RDDynamicUniformBufferPtr ubo;
    };

} // namespace sky
