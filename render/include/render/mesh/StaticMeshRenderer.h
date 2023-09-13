//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/RenderPrimitive.h>
#include <core/math/Matrix4.h>

namespace sky {

    class StaticMeshRenderer {
    public:
        StaticMeshRenderer() = default;
        ~StaticMeshRenderer() = default;

        void SetMesh(const RDMeshPtr &mesh);
        void UpdateTransform(const Matrix4 &matrix);

    private:
        RDMeshPtr mesh;

        std::unique_ptr<RenderPrimitive> primitive;
        rhi::VertexAssemblyPtr va;
        RDDynamicUniformBufferPtr ubo;
    };

} // namespace sky
