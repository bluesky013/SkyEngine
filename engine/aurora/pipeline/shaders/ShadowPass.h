#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_SHADOW_PASS_FIELDS(F, A, T) F(T, MAT4, lightViewProjection)

#define AURORA_SHADOW_PASS_RESOURCES(R) R(UNIFORM, ShadowPassParams, gShadowPass, 1, 0, AURORA_SHADOW_PASS_FIELDS)

AURORA_SHADER_PASS(sky::aurora::shared, ShadowPass, AURORA_SHADOW_PASS_RESOURCES)

#undef AURORA_SHADOW_PASS_RESOURCES
#undef AURORA_SHADOW_PASS_FIELDS
