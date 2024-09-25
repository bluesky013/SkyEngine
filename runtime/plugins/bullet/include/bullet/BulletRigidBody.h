//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/RigidBody.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btMotionState.h>
#include <memory>

namespace sky::phy {
    class BulletPhysicsWorld;

    class BulletRigidBody : public RigidBody, public btMotionState {
    public:
        BulletRigidBody() = default;
        ~BulletRigidBody() override = default;

        btRigidBody *BuildRigidBody();
        btRigidBody *GetRigidBody() const { return rigidBody.get(); }

        void SetPhysicsWorld(BulletPhysicsWorld *wd);

    private:
        void getWorldTransform(btTransform& worldTrans) const override;
        void setWorldTransform(const btTransform& worldTrans) override;

        void OnMassChanged() override;
        void OnShapeChanged() override;
        void OnFlagChanged() override;
        void OnGroupMaskChanged() override;

        void SetFlags();

        BulletPhysicsWorld *world = nullptr;
        std::unique_ptr<btCollisionShape> collisionShape;
        std::unique_ptr<btRigidBody> rigidBody;

        btVector3 localInertia = btVector3{0, 0, 0};
    };

} // namespace sky::phy
