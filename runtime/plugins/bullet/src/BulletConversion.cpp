//
// Created by blues on 2024/9/25.
//

#include <bullet/BulletConversion.h>

namespace sky::phy {

    btVector3 ToBullet(const Vector3& vec)
    {
        return btVector3{vec.x, vec.y, vec.z};
    }

    btQuaternion ToBullet(const Quaternion& quat)
    {
        return btQuaternion{quat.x, quat.y, quat.z, quat.w};
    }

    btTransform ToBullet(const Matrix4& mat)
    {
        Vector3 trans;
        Quaternion rot;
        Vector3 scale;
        Decompose(mat, trans, rot, scale);

        return btTransform{ToBullet(rot), ToBullet(trans)};
    }

    PHY_ScalarType ToBullet(const IndexType& idx)
    {
        switch (idx) {
            case IndexType::U16:
                return PHY_SHORT;
            case IndexType::U32:
                return PHY_INTEGER;
            default:
                break;
        }
        return PHY_INTEGER;
    }

    btTransform ToBullet(const Transform& mat)
    {
        return btTransform{ToBullet(mat.rotation), ToBullet(mat.translation)};
    }

    Quaternion FromBullet(const btQuaternion& quat)
    {
        return {quat.w(), quat.x(), quat.y(), quat.z()};
    }

    Vector3 FromBullet(const btVector3 &vec)
    {
        return {vec.x(), vec.y(), vec.z()};
    }

    Transform FromBullet(const btTransform &trans)
    {
        Transform res = Transform::GetIdentity();
        res.translation = FromBullet(trans.getOrigin());
        res.rotation = FromBullet(trans.getRotation());
        return res;
    }
} // namespace sky::phy