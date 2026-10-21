#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_SCENE_ENVIRONMENT_PASS_FIELDS(F, A, T) F(T, FLOAT4, environment)

#define AURORA_SCENE_ENVIRONMENT_PASS_RESOURCES(R)                                                                                                   \
    R(UNIFORM, SceneEnvironmentParams, gSceneEnvironment, 1, 4, AURORA_SCENE_ENVIRONMENT_PASS_FIELDS)                                                \
    R(TEXTURE, TEXTURE2D, FLOAT4, gDiffuseIrradiance, 1, 5)                                                                                          \
    R(TEXTURE, TEXTURE2D, FLOAT4, gSpecularEnvironment, 1, 6)                                                                                        \
    R(TEXTURE, TEXTURE2D, FLOAT4, gBrdfIntegration, 1, 7)                                                                                            \
    R(SAMPLER, REGULAR, gEnvironmentSampler, 1, 8)                                                                                                   \
    R(SAMPLER, REGULAR, gBrdfSampler, 1, 9)

AURORA_SHADER_PASS(SceneEnvironmentPass, AURORA_SCENE_ENVIRONMENT_PASS_RESOURCES)

#undef AURORA_SCENE_ENVIRONMENT_PASS_RESOURCES
#undef AURORA_SCENE_ENVIRONMENT_PASS_FIELDS
