//
// Created by blues on 2024/10/6.
//

#include <bullet/BulletCollisionObject.h>
#include <bullet/BulletShapes.h>
#include <bullet/BulletConversion.h>
#include <bullet/BulletPhysicsWorld.h>

#include <memory>

namespace sky::phy {

    BulletCollisionObject::BulletCollisionObject()
        : object(std::make_unique<btCollisionObject>())
    {
    }

    void BulletCollisionObject::SetPhysicsWorld(BulletPhysicsWorld *wd)
    {
        if (world != nullptr) {
            world->GetWorld()->removeCollisionObject(object.get());
        }
        world = wd;
        if (world != nullptr) {
            world->GetWorld()->addCollisionObject(object.get(),
                static_cast<int32_t>(group.value),
                static_cast<int32_t>(mask.value));
        }
    }

    void BulletCollisionObject::OnShapeChanged()
    {
        if (!physicsShape) {
            BoxShape shape = {};
            shape.halfExt = VEC3_ZERO;
            physicsShape = std::make_unique<PhysicsBoxShape>(shape);
        }
        object->setCollisionShape(static_cast<BulletShape*>(physicsShape->GetShape())->GetShape());
        object->setRestitution(1.f);
    }

    void BulletCollisionObject::OnGroupMaskChanged()
    {
        if (world != nullptr && object) {
            world->GetWorld()->removeCollisionObject(object.get());
            world->GetWorld()->addCollisionObject(object.get(),
                static_cast<int32_t>(group.value),
                static_cast<int32_t>(mask.value));
        }
    }

    void BulletCollisionObject::SetWorldTransform(const Transform &trans)
    {
        object->setWorldTransform(ToBullet(trans));
    }

    PhysicsWorld* BulletCollisionObject::GetWorld() const
    {
        return world;
    }

} // namespace sky::phy