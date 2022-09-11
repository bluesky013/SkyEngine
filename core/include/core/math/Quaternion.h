//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Math.h>

namespace sky {

    struct Quaternion {
        union {
            float v[4];
            struct {
                float x;
                float y;
                float z;
                float w;
            };
        };
    };

}