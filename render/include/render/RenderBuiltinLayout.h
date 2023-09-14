//
// Created by Zach Lee on 2023/9/8.
//

#pragma once

#include <core/math/Matrix4.h>

namespace sky {

    struct SceneViewInfo {
        Matrix4 worldMatrix;
        Matrix4 viewMatrix;
        Matrix4 projectMatrix;
        Matrix4 viewProjectMatrix;
    };

    struct InstanceLocal {
        Matrix4 worldMatrix;
        Matrix4 inverseTranspose;
    };

} // namespace sky
