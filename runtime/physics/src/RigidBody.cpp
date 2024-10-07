//
// Created by blues on 2024/9/1.
//

#include <physics/RigidBody.h>

namespace sky::phy {

    RigidBody::RigidBody()
        : physicsShape(std::make_unique<PhysicsBoxShape>(BoxShape{VEC3_ZERO, Vector3(0.01f, 0.01f, 0.01f)}))
    {
    }

    void RigidBody::SetShape(PhysicsShape *shape)
    {
        if (shape == nullptr) {
            physicsShape = std::make_unique<PhysicsBoxShape>(BoxShape{VEC3_ZERO, Vector3(0.01f, 0.01f, 0.01f)});
        } else {
            physicsShape.reset(shape);
        }
        OnShapeChanged();
    }

    void RigidBody::SetGroup(CollisionFilters group_)
    {
        group = group_;
        OnGroupMaskChanged();
    }

    void RigidBody::SetMask(CollisionFilters mask_)
    {
        mask = mask_;
        OnGroupMaskChanged();
    }

    void RigidBody::SetStartTrans(const Transform &trans)
    {
        startTrans = trans;
    }
} // namespace sky::phy