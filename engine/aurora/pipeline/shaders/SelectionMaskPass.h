//
// Created on 2026/10/23.
//

#pragma once
#include <aurora/shader/SharedShader.h>
#define AURORA_SELECTION_MASK_FIELDS(F, A, T) F(T, MAT4, viewProjection)
#define AURORA_SELECTION_MASK_RESOURCES(R)                                                                                                           \
    R(UNIFORM, SelectionMaskParams, gSelectionMask, 1, 0, AURORA_SELECTION_MASK_FIELDS)                                                              \
    R(TEXTURE, TEXTURE2D, FLOAT, gSceneDepth, 1, 1)
AURORA_SHADER_PASS(sky::aurora::shared, SelectionMaskPass, AURORA_SELECTION_MASK_RESOURCES)
#undef AURORA_SELECTION_MASK_RESOURCES
#undef AURORA_SELECTION_MASK_FIELDS
