//
// Created by Zach Lee on 2022/9/10.
//

#pragma once

namespace sky {

    struct Color {
        union {
            float v[4];
            struct {
                float r;
                float g;
                float b;
                float a;
            };
        };

        Color();
        Color(float r_, float g_, float b_, float a_);
    };
}
