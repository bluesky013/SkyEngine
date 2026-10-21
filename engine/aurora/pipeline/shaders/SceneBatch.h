#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_SCENE_BATCH_FIELDS(F, A, T)                                                                                                           \
    F(T, FLOAT4, tint)                                                                                                                               \
    F(T, MAT4, world)

#define AURORA_SCENE_BATCH_RESOURCES(R) R(UNIFORM, SceneBatchParams, gSceneBatch, 2, 0, AURORA_SCENE_BATCH_FIELDS)

AURORA_SHADER_PASS(SceneBatch, AURORA_SCENE_BATCH_RESOURCES)

#undef AURORA_SCENE_BATCH_RESOURCES
#undef AURORA_SCENE_BATCH_FIELDS
