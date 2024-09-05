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

    // https://gist.github.com/cmf028/81e8d3907035640ee0e3fdd69ada543f
    AABB AABB::Transform(const AABB& box, const Matrix4 &mtx)
    {
        // compute column multiplies for the AABB min
        Vector3 min_c1 = Cast(mtx.m[0]) * box.min.x;
        Vector3 min_c2 = Cast(mtx.m[1]) * box.min.y;
        Vector3 min_c3 = Cast(mtx.m[2]) * box.min.z + Cast(mtx.m[3]); // place 4th column add here for free add (MAD)

        // compute column multiplies for the AABB max
        Vector3 max_c1 = Cast(mtx.m[0]) * box.max.x;
        Vector3 max_c2 = Cast(mtx.m[1]) * box.max.y;
        Vector3 max_c3 = Cast(mtx.m[2]) * box.max.z + Cast(mtx.m[3]); // place 4th column add here for free add (MAD)

        // minimize and maximize the resulting transforms
        Vector3 tMin = Min(min_c1,max_c1) + Min(min_c2, max_c2) + Min(min_c3, max_c3);
        Vector3 tMax = Max(min_c1,max_c1) + Max(min_c2, max_c2) + Max(min_c3, max_c3);

        AABB res;
        res.min = tMin;
        res.max = tMax;
        return res;
    }

} // namespace sky
