//
// Created by Zach Lee on 2022/7/19.
//

#pragma once
#include <core/math/Matrix.h>
#include <core/logger/Logger.h>
#include <render/resources/Mesh.h>

namespace sky {

    inline void PrintMatrix(const Matrix4& m)
    {
        LOG_I("mtx", "\n[%f, %f, %f, %f]\n[%f, %f, %f, %f]\n[%f, %f, %f, %f]\n[%f, %f, %f, %f]\n",
              m[0][0], m[0][1], m[0][2], m[0][3],
              m[1][0], m[1][1], m[1][2], m[1][3],
              m[2][0], m[2][1], m[2][2], m[2][3],
              m[3][0], m[3][1], m[3][2], m[3][3]);
    }

    class RenderMesh {
    public:
        RenderMesh() = default;
        virtual ~RenderMesh() = default;

        void SetWorldMatrix(const Matrix4& matrix)
        {
            worldMatrix = matrix;
            inverseTransposeMatrix = glm::inverseTranspose(worldMatrix);
            PrintMatrix(worldMatrix);
            PrintMatrix(inverseTransposeMatrix);
        }

    private:

        Matrix4 worldMatrix;
        Matrix4 inverseTransposeMatrix;
    };
    using RenderMeshPtr = std::shared_ptr<RenderMesh>;

}