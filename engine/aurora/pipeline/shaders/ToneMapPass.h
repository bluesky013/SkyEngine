#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_TONE_MAP_PASS_FIELDS(F, A, T) F(T, FLOAT4, display)

#define AURORA_TONE_MAP_PASS_RESOURCES(R)                                                                                                            \
    R(UNIFORM, ToneMapParams, gToneMap, 1, 0, AURORA_TONE_MAP_PASS_FIELDS)                                                                           \
    R(TEXTURE, TEXTURE2D, FLOAT4, gHdrColor, 1, 1)                                                                                                   \
    R(SAMPLER, REGULAR, gHdrSampler, 1, 2)

AURORA_SHADER_PASS(ToneMapPass, AURORA_TONE_MAP_PASS_RESOURCES)

#undef AURORA_TONE_MAP_PASS_RESOURCES
#undef AURORA_TONE_MAP_PASS_FIELDS
