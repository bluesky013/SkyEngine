//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletRigidBody.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>

namespace sky::phy {

    btRigidBody *BulletRigidBody::BuildRigidBody()
    {
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, this, collisionShape.get());
        rigidBody = std::make_unique<btRigidBody>(rbInfo);

        SetFlags();

        return rigidBody.get();
    }

    void BulletRigidBody::SetFlags()
    {
        int flags = 0;
        switch (collisionFlag) {
            case CollisionFlag::STATIC:
                flags |= btCollisionObject::CF_STATIC_OBJECT;
                break;
            case CollisionFlag::DYNAMIC:
                flags |= btCollisionObject::CF_DYNAMIC_OBJECT;
                break;
            case CollisionFlag::KINEMATIC:
                flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
                break;
        }
        rigidBody->setCollisionFlags(flags);
    }

    void BulletRigidBody::getWorldTransform(btTransform& worldTrans) const
    {

    }

    void BulletRigidBody::setWorldTransform(const btTransform& worldTrans)
    {

    }

} // namespace sky::phy