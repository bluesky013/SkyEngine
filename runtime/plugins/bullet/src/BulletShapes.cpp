//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletShapes.h>
#include <bullet/BulletConversion.h>

namespace sky::phy {

    BulletShape::BulletShape(const BoxShape &shape)
    {
        auto *boxShape = new btBoxShape(ToBullet(shape.halfExt));
        if (shape.pivot.x == 0.f && shape.pivot.y == 0.f && shape.pivot.z == 0.f) {
            collisionShape.reset(boxShape);
        } else {
            baseShape.reset(boxShape);
            auto* compound = new btCompoundShape();
            btTransform trans{btQuaternion::getIdentity(), ToBullet(shape.pivot)};
            compound->addChildShape(trans, baseShape.get());
            collisionShape.reset(compound);
        }
    }

    BulletShape::BulletShape(const SphereShape &shape)
    {
        auto *sphereShape = new btSphereShape(shape.radius);
        if (shape.pivot.x == 0.f && shape.pivot.y == 0.f && shape.pivot.z == 0.f) {
            collisionShape.reset(sphereShape);
        } else {
            baseShape.reset(sphereShape);
            auto* compound = new btCompoundShape();
            btTransform trans{btQuaternion::getIdentity(), ToBullet(shape.pivot)};
            compound->addChildShape(trans, baseShape.get());
            collisionShape.reset(compound);
        }
    }

} // namespace sky::phy