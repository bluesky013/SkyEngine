//
// Created by blues on 2024/10/6.
//

#pragma once

#include <physics/CollisionObject.h>
#include <btBulletCollisionCommon.h>
#include <memory>

namespace sky::phy {
    class BulletPhysicsWorld;

    class BulletCollisionObject : public CollisionObject {
    public:
        BulletCollisionObject();
        ~BulletCollisionObject() override = default;

        void SetPhysicsWorld(BulletPhysicsWorld *wd);
    private:
        void SetWorldTransform(const Transform &trans) override;
        PhysicsWorld* GetWorld() const override;
        void OnShapeChanged() override;
        void OnGroupMaskChanged() override;

        std::unique_ptr<btCollisionObject> object;
        BulletPhysicsWorld* world = nullptr;
    };

} // namespace sky::phy
