//
// Created by Zach Lee on 2022/6/21.
//

#pragma once

#define SKY_DISABLE_COPY(className)                     \
    className(const className &)            = delete;   \
    className &operator=(const className &) = delete;

#define SKY_COMBINE_CH_U32(a, b, c, d) \
    (uint32_t)(a & 0xFF) | ((uint32_t)(b & 0xFF) << 8) | ((uint32_t)(c & 0xFF) << 16) | ((uint32_t)(d & 0xFF) << 24)