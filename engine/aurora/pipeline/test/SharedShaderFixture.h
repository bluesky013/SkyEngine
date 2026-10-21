#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_TEST_FIELDS(F, A, T)                                                                                                                  \
    F(T, FLOAT, scalar)                                                                                                                              \
    F(T, INT, signedValue)                                                                                                                           \
    F(T, UINT, unsignedValue)                                                                                                                        \
    F(T, UINT, enabled)                                                                                                                              \
    F(T, FLOAT2, pairA)                                                                                                                              \
    F(T, FLOAT2, pairB)                                                                                                                              \
    F(T, FLOAT4, directionWeight)                                                                                                                    \
    F(T, MAT4, transform)                                                                                                                            \
    A(T, FLOAT4, values, 2)
#define AURORA_TEST_RESOURCES(R) R(UNIFORM, SharedTestParams, gTest, 1, 0, AURORA_TEST_FIELDS)
AURORA_SHADER_PASS(SharedTest, AURORA_TEST_RESOURCES)
#undef AURORA_TEST_RESOURCES
#undef AURORA_TEST_FIELDS

#define AURORA_TEST_ONLY_RESOURCES(R)                                                                                                                \
    R(TEXTURE, TEXTURE2D, FLOAT4, gLayer, 2, 0)                                                                                                      \
    R(SAMPLER, REGULAR, gSampling, 2, 1)
AURORA_SHADER_PASS(SharedResourceTest, AURORA_TEST_ONLY_RESOURCES)
#undef AURORA_TEST_ONLY_RESOURCES
