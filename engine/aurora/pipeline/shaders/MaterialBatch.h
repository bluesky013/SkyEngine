#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_MATERIAL_BATCH_FIELDS(F, A, T) F(T, FLOAT4, tint) F(T, FLOAT4, surface)
#define AURORA_MATERIAL_BATCH_RESOURCES(R)    R(UNIFORM, MaterialBatchParams, gMaterial, 2, 2, AURORA_MATERIAL_BATCH_FIELDS)

AURORA_SHADER_PASS(sky::aurora::shared, MaterialBatch, AURORA_MATERIAL_BATCH_RESOURCES)

#undef AURORA_MATERIAL_BATCH_RESOURCES
#undef AURORA_MATERIAL_BATCH_FIELDS
