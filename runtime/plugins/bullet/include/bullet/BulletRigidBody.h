//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/RigidBody.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btMotionState.h>
#include <memory>

namespace sky::phy {

    class BulletRigidBody : public RigidBody, public btMotionState {
    public:
        BulletRigidBody() = default;
        ~BulletRigidBody() override = default;

        btRigidBody *BuildRigidBody();
        btRigidBody *GetRigidBody() const { return rigidBody.get(); }
    private:
        void getWorldTransform(btTransform& worldTrans) const override;
        void setWorldTransform(const btTransform& worldTrans) override;

        void SetFlags();

        std::unique_ptr<btCollisionShape> collisionShape;
        std::unique_ptr<btRigidBody> rigidBody;
    };

} // namespace sky::phy
