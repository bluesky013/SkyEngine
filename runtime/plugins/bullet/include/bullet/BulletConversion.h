//
// Created by blues on 2024/9/25.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Matrix4.h>
#include <core/math/MathUtil.h>
#include <core/math/Transform.h>
#include <rhi/Core.h>
#include <btBulletCollisionCommon.h>

namespace sky::phy {

    btVector3 ToBullet(const Vector3& vec);
    btQuaternion ToBullet(const Quaternion& quat);
    btTransform ToBullet(const Matrix4& mat);
    btTransform ToBullet(const Transform& mat);
    PHY_ScalarType ToBullet(const rhi::IndexType& idx);

    Quaternion FromBullet(const btQuaternion& quat);
    Vector3 FromBullet(const btVector3 &vec);
    Transform FromBullet(const btTransform &trans);

} // namespace sky::phy
