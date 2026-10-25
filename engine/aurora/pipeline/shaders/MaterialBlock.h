#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_MATERIAL_BLOCK_FIELDS(F, A, T) F(T, FLOAT4, tint) F(T, FLOAT4, surface)
#define AURORA_MATERIAL_BLOCK_RESOURCES(R)    R(UNIFORM, MaterialBlockParams, gMaterial, 2, 0, AURORA_MATERIAL_BLOCK_FIELDS)

AURORA_SHADER_PASS(sky::aurora::shared, MaterialBlock, AURORA_MATERIAL_BLOCK_RESOURCES)


#undef AURORA_MATERIAL_BLOCK_RESOURCES
#undef AURORA_MATERIAL_BLOCK_FIELDS
