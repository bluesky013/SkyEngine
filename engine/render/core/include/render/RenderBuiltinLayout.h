//
// Created by Zach Lee on 2023/9/8.
//

#pragma once

#include <core/math/Matrix4.h>

namespace sky {

    static constexpr uint32_t MAX_POINT_LIGHTS = 8;
    static constexpr uint32_t MAX_SPOT_LIGHTS  = 4;

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
        uint32_t pointLightCount;
        uint32_t spotLightCount;
        uint32_t lightPadding[2];
    };

    struct ShaderPointLightData {
        Vector4 positionRange;    // xyz: world position, w: influence range
        Vector4 colorIntensity;   // xyz: linear RGB color, w: luminous intensity (candela)
    };

    struct ShaderSpotLightData {
        Vector4 positionRange;    // xyz: world position, w: influence range
        Vector4 directionInner;   // xyz: normalized direction, w: cos(innerConeAngle)
        Vector4 colorIntensity;   // xyz: linear RGB color, w: luminous intensity (candela)
        Vector4 outerAnglePad;    // x: cos(outerConeAngle), yzw: reserved
    };

    struct ShaderPointLightBuffer {
        ShaderPointLightData lights[MAX_POINT_LIGHTS];
    };

    struct ShaderSpotLightBuffer {
        ShaderSpotLightData lights[MAX_SPOT_LIGHTS];
    };

} // namespace sky
