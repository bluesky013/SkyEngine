//
// Created by blues on 2024/10/6.
//

#include <physics/CollisionObject.h>

namespace sky::phy {

    void CollisionObject::SetShape(PhysicsShape *shape)
    {
        physicsShape.reset(shape);
        OnShapeChanged();
    }

    void CollisionObject::SetGroup(const CollisionFilters& group_)
    {
        group = group_;
        OnGroupMaskChanged();
    }

    void CollisionObject::SetMask(const CollisionFilters& mask_)
    {
        mask = mask_;
        OnGroupMaskChanged();
    }
} // namespace sky::phy