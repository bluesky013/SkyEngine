#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_TEXTURE_TO_SCREEN_PASS_FIELDS(F, A, T) F(T, FLOAT4, Misc)

#define AURORA_TEXTURE_TO_SCREEN_PASS_RESOURCES(R)                                                                                                   \
    R(UNIFORM, TextureToScreenParams, gTextureToScreen, 1, 0, AURORA_TEXTURE_TO_SCREEN_PASS_FIELDS)                                                  \
    R(TEXTURE, TEXTURE2D, FLOAT4, gSourceTexture, 1, 1)                                                                                              \
    R(SAMPLER, REGULAR, gSourceSampler, 1, 2)

AURORA_SHADER_PASS(TextureToScreenPass, AURORA_TEXTURE_TO_SCREEN_PASS_RESOURCES)

#undef AURORA_TEXTURE_TO_SCREEN_PASS_RESOURCES
#undef AURORA_TEXTURE_TO_SCREEN_PASS_FIELDS
