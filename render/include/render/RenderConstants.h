//
// Created by Zach Lee on 2022/7/30.
//

#pragma once
#include <cstdint>

namespace sky {

    constexpr uint32_t MAX_RENDER_SCENE = 2;
    constexpr uint32_t MAX_ACTIVE_CAMERA = 1;

    constexpr uint32_t MAIN_CAMERA_TAG = 0x01;

    constexpr uint32_t DEFAULT_OBJECT_SET_NUM = 200;

    constexpr uint32_t INFLIGHT_FRAME = 1;

    constexpr uint32_t MIN_MATERIAL_BUFFER_STRIDE    = 16;
    constexpr uint32_t MATERIAL_BUFFER_LEVEL_OFFSET  = 4;  // log2(MIN_MATERIAL_BUFFER_STRIDE)
    constexpr uint32_t MAX_MATERIAL_BUFFER_SIZE      = 1024;
    constexpr uint32_t MAX_MATERIAL_COUNT_PER_BLOCK  = 32;
    constexpr uint32_t DEFAULT_MATERIAL_BUFFER_BLOCK = 4096;
}