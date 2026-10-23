//
// Created on 2026/10/23.
//

#pragma once
#include <aurora/shader/SharedShader.h>
#define AURORA_SELECTION_OUTLINE_FIELDS(F, A, T) F(T, FLOAT4, visible) F(T, FLOAT4, occluded) F(T, FLOAT4, metrics)
#define AURORA_SELECTION_OUTLINE_RESOURCES(R)                                                                                                        \
    R(UNIFORM, SelectionOutlineParams, gSelectionOutline, 1, 0, AURORA_SELECTION_OUTLINE_FIELDS)                                                     \
    R(TEXTURE, TEXTURE2D, FLOAT4, gSelectionImage, 1, 1)
AURORA_SHADER_PASS(sky::aurora::shared, SelectionOutlinePass, AURORA_SELECTION_OUTLINE_RESOURCES)
#undef AURORA_SELECTION_OUTLINE_RESOURCES
#undef AURORA_SELECTION_OUTLINE_FIELDS
