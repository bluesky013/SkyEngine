//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletRigidBody.h>
#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletShapes.h>
#include <bullet/BulletConversion.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>

namespace sky::phy {

    BulletRigidBody::BulletRigidBody()
    {
        BuildRigidBody();
    }

    void BulletRigidBody::BuildRigidBody()
    {
        auto *shape = static_cast<BulletShape*>(physicsShape->GetShape())->GetShape();

        btVector3 localInertia(0.f, 0.f, 0.f);
        if (mass != 0.f) {
            shape->calculateLocalInertia(mass, localInertia);
        }

        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, this, shape, localInertia);
        rbInfo.m_startWorldTransform = ToBullet(startTrans);
        rigidBody = std::make_unique<btRigidBody>(rbInfo);
        SetFlagsImpl();
        SetPhysicsMat();
    }

    void BulletRigidBody::SetFlagsImpl()
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

    void BulletRigidBody::SetPhysicsMat()
    {
        rigidBody->setRestitution(0.5f);
    }

    void BulletRigidBody::SetPhysicsWorld(BulletPhysicsWorld *wd)
    {
        if (world != nullptr) {
            world->GetWorld()->removeRigidBody(rigidBody.get());
        }
        world = wd;
        if (world != nullptr) {
            world->GetWorld()->addRigidBody(rigidBody.get(), static_cast<int32_t>(group.value), static_cast<int32_t>(mask));
        }
    }

    void BulletRigidBody::SetMass(float m)
    {
        mass = m;

        btVector3 localInertia(0.f, 0.f, 0.f);
        if (mass != 0.f) {
            auto *shape = static_cast<BulletShape*>(physicsShape->GetShape())->GetShape();
            shape->calculateLocalInertia(mass, localInertia);
        }
        rigidBody->setMassProps(mass, localInertia);
    }

    void BulletRigidBody::SetFlag(CollisionFlag m)
    {
        collisionFlag = m;
        SetFlagsImpl();
    }

    void BulletRigidBody::OnShapeChanged()
    {
        if (world != nullptr) {
            world->GetWorld()->removeRigidBody(rigidBody.get());
            BuildRigidBody();
            world->GetWorld()->addRigidBody(rigidBody.get(), static_cast<int32_t>(group.value), static_cast<int32_t>(mask));
        }
    }

    void BulletRigidBody::OnGroupMaskChanged()
    {
        if (world != nullptr) {
            world->GetWorld()->removeRigidBody(rigidBody.get());
            world->GetWorld()->addRigidBody(rigidBody.get(), static_cast<int32_t>(group.value), static_cast<int32_t>(mask));
        }
    }

    void BulletRigidBody::getWorldTransform(btTransform& worldTrans) const
    {
        worldTrans = ToBullet(startTrans);
    }

    void BulletRigidBody::setWorldTransform(const btTransform& worldTrans)
    {
        if (listener != nullptr) {
            listener->OnRigidBodyUpdate(FromBullet(worldTrans));
        }
    }

} // namespace sky::phy