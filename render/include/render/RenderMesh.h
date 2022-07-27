//
// Created by Zach Lee on 2022/7/19.
//

#pragma once
#include <core/math/Matrix.h>
#include <render/resources/Mesh.h>

namespace sky {

    class RenderMesh {
    public:
        RenderMesh() = default;
        virtual ~RenderMesh() = default;

        inline void SetWorldMatrix(const Matrix4& matrix) { worldMatrix = matrix; }

    private:
        Matrix4 worldMatrix;
    };
    using RenderMeshPtr = std::shared_ptr<RenderMesh>;

}