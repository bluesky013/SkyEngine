#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_SCENE_LIGHTING_PASS_FIELDS(F, A, T)                                                                                                   \
    F(T, FLOAT4, lightCount)                                                                                                                         \
    A(T, FLOAT4, positions, 8)                                                                                                                       \
    A(T, FLOAT4, directions, 8)                                                                                                                      \
    A(T, FLOAT4, colors, 8)                                                                                                                          \
    A(T, FLOAT4, cones, 8)                                                                                                                           \
    F(T, MAT4, shadowViewProjection)                                                                                                                 \
    F(T, FLOAT4, shadowControl)

#define AURORA_SCENE_LIGHTING_PASS_RESOURCES(R)                                                                                                      \
    R(UNIFORM, SceneLightingParams, gSceneLighting, 1, 1, AURORA_SCENE_LIGHTING_PASS_FIELDS)                                                         \
    R(TEXTURE, TEXTURE2D, FLOAT, gShadowImage, 1, 2)                                                                                                 \
    R(SAMPLER, REGULAR, gShadowSampler, 1, 3)

AURORA_SHADER_PASS(SceneLightingPass, AURORA_SCENE_LIGHTING_PASS_RESOURCES)

#undef AURORA_SCENE_LIGHTING_PASS_RESOURCES
#undef AURORA_SCENE_LIGHTING_PASS_FIELDS
