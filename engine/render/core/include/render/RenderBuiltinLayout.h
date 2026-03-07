//
// Created by Zach Lee on 2023/9/8.
//

#pragma once

#include <core/math/Matrix4.h>

namespace sky {

    // Tile-based shadow map constants
    static constexpr uint32_t TILE_SHADOW_MAX_LIGHTS  = 4;
    static constexpr uint32_t TILE_SHADOW_TILE_SIZE   = 16;
    static constexpr uint32_t TILE_SHADOW_MAX_TILES_X = 128; // 128 * 16 = 2048px max width
    static constexpr uint32_t TILE_SHADOW_MAX_TILES_Y = 72;  // 72  * 16 = 1152px max height
    static constexpr uint32_t TILE_SHADOW_MAX_TILES   = TILE_SHADOW_MAX_TILES_X * TILE_SHADOW_MAX_TILES_Y;

    // Per-shadow-light data uploaded to the GPU
    struct ShadowLightData {
        Matrix4  lightViewProj;    // Light's view-projection matrix
        Vector4  posRadius;        // xyz = light position, w = radius (0 for directional)
        int32_t  lightType;        // 0 = directional, 1 = spot, 2 = point
        float    pad[3];
    };

    // Uploaded once per frame to describe all active shadow lights and tile counts
    struct TileShadowPassInfo {
        ShadowLightData lights[TILE_SHADOW_MAX_LIGHTS]; // One entry per active shadow light
        uint32_t lightCount;   // Number of active shadow lights
        uint32_t tileCountX;   // Number of tiles in X direction
        uint32_t tileCountY;   // Number of tiles in Y direction
        float    pad;
    };

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
