//
// Created by Zach Lee on 2022/7/19.
//

#pragma once
#include <core/math/Matrix.h>

namespace sky {

    class RenderMesh {
    public:
        RenderMesh() = default;
        virtual ~RenderMesh() = default;

    private:
        Matrix4 worldMatrix;
    };

}