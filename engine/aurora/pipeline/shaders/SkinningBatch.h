#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_SKINNING_BATCH_RESOURCES(R) R(STRUCTURED, FLOAT4, gSkinningPalette, 3, 3)

AURORA_SHADER_PASS(sky::aurora::shared, SkinningBatch, AURORA_SKINNING_BATCH_RESOURCES)

#undef AURORA_SKINNING_BATCH_RESOURCES
