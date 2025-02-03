//
// Created by Zach Lee on 2023/9/8.
//

#pragma once

#include <core/math/Matrix4.h>

namespace sky {
    struct SceneViewInfo {
        Matrix4 world;
        Matrix4 view;
        Matrix4 project;
        Matrix4 viewProject;
    };

    struct InstanceLocal {
        Matrix4 worldMatrix;
        Matrix4 inverseTranspose;
    };

    struct ShaderPassInfo {
        Matrix4 lightMatrix;
        Vector4 mainLightColor;
        Vector3 mainLightDirection;
        float   padding;
        Vector4 viewport;
    };

} // namespace sky
