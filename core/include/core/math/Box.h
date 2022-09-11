//
// Created by Zach Lee on 2022/5/29.
//

#pragma once

#include <core/math/MathUtil.h>

namespace sky {

    struct Box {
        Vector3 min;
        Vector3 max;

        Box Combine(const Box &rhs)
        {
            Box res = {};
            res.min = Min(min, rhs.min);
            res.max = Max(max, rhs.max);
            return res;
        }

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(min, max);
        }
    };

} // namespace sky
