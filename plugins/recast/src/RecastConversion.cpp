//
// Created by blues on 2024/10/7.
//

#include <recast/RecastConversion.h>

namespace sky::ai {

    void ToRecast(const Vector3& vec, float* out)
    {
        out[0] = vec.x;
        out[1] = vec.y;
        out[2] = vec.z;
    }

} // namespace sky::ai