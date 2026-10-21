#pragma once
#include <aurora/shader/SharedShader.h>

#define FIRST_GLOBAL(F, A, T) F(T, FLOAT4, color)
#define FIRST_PASS(F, A, T)   F(T, FLOAT4, color)
#define FIRST_BATCH(F, A, T)  F(T, FLOAT4, color)
#define FIRST_RESOURCES(R)                                                                                                                           \
    R(UNIFORM, GlobalParams, gGlobal, 0, 0, FIRST_GLOBAL)                                                                                            \
    R(UNIFORM, ToonParams, gPass, 1, 0, FIRST_PASS)                                                                                                  \
    R(UNIFORM, BatchParams, gBatch, 2, 0, FIRST_BATCH)
AURORA_SHADER_PASS(test_pipeline::first, ToonPass, FIRST_RESOURCES)
#undef FIRST_RESOURCES
#undef FIRST_BATCH
#undef FIRST_PASS
#undef FIRST_GLOBAL
