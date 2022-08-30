//
// Created by Zach Lee on 2022/5/29.
//

#pragma once

#include <core/math/Vector.h>

namespace sky {

    struct Box {
        Vector3 min = glm::zero<Vector3>();
        Vector3 max = glm::zero<Vector3>();

        Box Combine(const Box &rhs)
        {
            Box res = {};
            res.min = glm::min(min, rhs.min);
            res.max = glm::max(max, rhs.max);
            return res;
        }

        template <class Archive> void serialize(Archive &ar)
        {
            ar(min, max);
        }
    };

} // namespace sky
