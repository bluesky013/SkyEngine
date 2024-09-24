//
// Created by blues on 2024/9/25.
//

#include <bullet/BulletConversion.h>

namespace sky::phy {

    btVector3 ToBullet(const Vector3& vec)
    {
        return btVector3{vec.x, vec.y, vec.z};
    }

} // namespace sky::phy