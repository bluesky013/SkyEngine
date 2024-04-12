//
// Created by Zach Lee on 2023/8/29.
//

#include <core/shapes/AABB.h>
#include <core/math/MathUtil.h>

namespace sky {

    void Merge(const AABB &a, const AABB &b, AABB &out)
    {
        out.min = Min(a.min, b.min);
        out.max = Max(a.max, b.max);
    }

} // namespace sky
