//
// Created by Zach Lee on 2022/7/19.
//

#pragma once
#include <core/math/Matrix.h>
#include <core/logger/Logger.h>
#include <render/resources/Mesh.h>

namespace sky {

    struct ObjectInfo {
        Matrix4 worldMatrix;
        Matrix4 inverseTransposeMatrix;
    };

    class RenderMesh {
    public:
        RenderMesh() = default;
        virtual ~RenderMesh() = default;

        inline void SetWorldMatrix(const Matrix4& matrix)
        {
            objectInfo.worldMatrix = matrix;
            objectInfo.inverseTransposeMatrix = glm::inverseTranspose(matrix);
        }

        inline void SetViewTag(uint32_t tag)
        {
            viewMask |= tag;
        }

    private:
        ObjectInfo objectInfo;
        uint32_t viewMask = 0;
    };
    using RenderMeshPtr = std::shared_ptr<RenderMesh>;

}