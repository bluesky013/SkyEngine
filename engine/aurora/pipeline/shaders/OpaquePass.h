#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_OPAQUE_PASS_FIELDS(F, A, T) F(T, MAT4, viewProjection) F(T, FLOAT4, cameraPosition)

#define AURORA_OPAQUE_PASS_RESOURCES(R) R(UNIFORM, OpaquePassParams, gOpaquePass, 1, 0, AURORA_OPAQUE_PASS_FIELDS)

AURORA_SHADER_PASS(sky::aurora::shared, OpaquePass, AURORA_OPAQUE_PASS_RESOURCES)

#undef AURORA_OPAQUE_PASS_RESOURCES
#undef AURORA_OPAQUE_PASS_FIELDS
