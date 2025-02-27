//
// Created by Zach Lee on 2023/9/8.
//

#pragma once

#include <core/math/Matrix4.h>

namespace sky {
    struct SceneViewInfo {
        Matrix4 world;
        Matrix4 view;
        Matrix4 viewProject;
        Matrix4 lastViewProject;
    };

    struct InstanceLocal {
        Matrix4 worldMatrix;
        Matrix4 inverseTranspose;
    };

    struct MeshletInfo {
        uint32_t firstMeshlet;
        uint32_t meshletCount;
        uint32_t firstInstance;
        uint32_t padding;
    };

    struct ShaderPassInfo {
        Matrix4 lightMatrix;
        Vector4 mainLightColor;
        Vector3 mainLightDirection;
        float   padding;
        Vector4 viewport;
    };

} // namespace sky
