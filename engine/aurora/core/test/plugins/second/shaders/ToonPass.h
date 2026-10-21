#pragma once
#include <aurora/shader/SharedShader.h>

#define SECOND_GLOBAL(F, A, T) F(T, MAT4, transform)
#define SECOND_PASS(F, A, T)   F(T, MAT4, viewProjection) F(T, FLOAT4, color)
#define SECOND_BATCH(F, A, T)  F(T, FLOAT4, color) F(T, FLOAT4, surface)
#define SECOND_RESOURCES(R)                                                                                                                          \
    R(UNIFORM, GlobalParams, gGlobal, 0, 3, SECOND_GLOBAL)                                                                                           \
    R(UNIFORM, ToonParams, gPass, 1, 2, SECOND_PASS)                                                                                                 \
    R(UNIFORM, BatchParams, gBatch, 2, 1, SECOND_BATCH)
AURORA_SHADER_PASS(test_pipeline::second, ToonPass, SECOND_RESOURCES)
#undef SECOND_RESOURCES
#undef SECOND_BATCH
#undef SECOND_PASS
#undef SECOND_GLOBAL
