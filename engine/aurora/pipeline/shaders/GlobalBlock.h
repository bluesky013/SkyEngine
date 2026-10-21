#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_GLOBAL_BLOCK_FIELDS(F, A, T)                                                                                                          \
    F(T, MAT4, view)                                                                                                                                 \
    F(T, MAT4, proj)                                                                                                                                 \
    F(T, MAT4, viewProj)                                                                                                                             \
    F(T, FLOAT4, cameraPos)

#define AURORA_GLOBAL_BLOCK_RESOURCES(R) R(UNIFORM, GlobalParams, gGlobal, 0, 0, AURORA_GLOBAL_BLOCK_FIELDS)

AURORA_SHADER_PASS(GlobalBlock, AURORA_GLOBAL_BLOCK_RESOURCES)

#undef AURORA_GLOBAL_BLOCK_RESOURCES
#undef AURORA_GLOBAL_BLOCK_FIELDS
