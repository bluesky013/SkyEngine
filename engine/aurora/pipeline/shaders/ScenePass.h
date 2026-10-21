#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_SCENE_PASS_FIELDS(F, A, T)                                                                                                            \
    F(T, MAT4, viewProjection)                                                                                                                       \
    F(T, FLOAT4, cameraPosition)

#define AURORA_SCENE_PASS_RESOURCES(R) R(UNIFORM, ScenePassParams, gScenePass, 1, 0, AURORA_SCENE_PASS_FIELDS)

AURORA_SHADER_PASS(sky::aurora::shared, ScenePass, AURORA_SCENE_PASS_RESOURCES)

#undef AURORA_SCENE_PASS_RESOURCES
#undef AURORA_SCENE_PASS_FIELDS
