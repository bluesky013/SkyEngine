#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_SCENE_BATCH_FIELDS(F, A, T) F(T, MAT4, world)

#define AURORA_SCENE_BATCH_RESOURCES(R) R(UNIFORM, SceneBatchParams, gSceneBatch, 3, 0, AURORA_SCENE_BATCH_FIELDS)

AURORA_SHADER_PASS(sky::aurora::shared, SceneBatch, AURORA_SCENE_BATCH_RESOURCES)

#undef AURORA_SCENE_BATCH_RESOURCES
#undef AURORA_SCENE_BATCH_FIELDS
