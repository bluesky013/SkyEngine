//
// Created by blues on 2024/9/1.
//

#include <physics/RigidBody.h>

namespace sky::phy {

    void RigidBody::AddShape(PhysicsShape* shape)
    {
        shapes.emplace_back(shape);
        OnShapeChanged();
    }

    void RigidBody::SetFlag(CollisionFlag flag)
    {
        collisionFlag = flag;
        OnFlagChanged();
    }

    void RigidBody::SetMass(float m)
    {
        mass = m;
        OnMassChanged();
    }

    void RigidBody::SetGroup(int32_t group_)
    {
        group = group_;
        OnGroupMaskChanged();
    }

    void RigidBody::SetMask(int32_t mask_)
    {
        mask = mask_;
        OnGroupMaskChanged();
    }
} // namespace sky::phy