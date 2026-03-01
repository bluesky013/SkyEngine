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
        BulletRigidBody();
        ~BulletRigidBody() override = default;

        void SetPhysicsWorld(BulletPhysicsWorld *wd);

        void SetMass(float m) override;
        void SetFlag(CollisionFlag m) override;

    private:
        void getWorldTransform(btTransform& worldTrans) const override;
        void setWorldTransform(const btTransform& worldTrans) override;

        void OnShapeChanged() override;
        void OnGroupMaskChanged() override;

        void BuildRigidBody();
        void SetFlagsImpl();
        void SetPhysicsMat();

        BulletPhysicsWorld *world = nullptr;
        std::unique_ptr<btRigidBody> rigidBody;
    };

} // namespace sky::phy
