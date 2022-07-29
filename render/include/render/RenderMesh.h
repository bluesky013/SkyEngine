//
// Created by Zach Lee on 2022/7/19.
//

#pragma once
#include <core/math/Matrix.h>
#include <core/logger/Logger.h>
#include <render/resources/Mesh.h>

namespace sky {

    class RenderMesh {
    public:
        RenderMesh() = default;
        virtual ~RenderMesh() = default;

        void SetWorldMatrix(const Matrix4& matrix)
        {
            worldMatrix = matrix;
            inverseTransposeMatrix = glm::inverseTranspose(worldMatrix);
        }

    private:
        Matrix4 worldMatrix;
        Matrix4 inverseTransposeMatrix;
    };
    using RenderMeshPtr = std::shared_ptr<RenderMesh>;

}