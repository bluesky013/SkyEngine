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
        Matrix4 invViewProject;
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

    struct HeightFogParams {
        Vector4 fogColor;          // rgb: fog color, a: unused
        Vector4 inscatterColor;    // rgb: inscatter color (sun/sky color), a: unused
        float   fogDensity;        // global fog density
        float   heightFalloff;     // how quickly fog increases with lower altitude
        float   baseHeight;        // height below which fog starts
        float   maxHeight;         // height above which there is no fog
        float   startDistance;     // distance before fog starts
        float   inscatterExponent; // exponent for directional inscattering
        float   padding0;
        float   padding1;
    };

} // namespace sky
