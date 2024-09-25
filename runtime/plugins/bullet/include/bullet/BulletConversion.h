//
// Created by blues on 2024/9/25.
//

#pragma once

#include <core/math/Vector3.h>
#include <btBulletCollisionCommon.h>

namespace sky::phy {

    btVector3 ToBullet(const Vector3& vec);

} // namespace sky::phy
