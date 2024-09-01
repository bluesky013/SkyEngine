//
// Created by blues on 2024/9/1.
//

#include <physics/RigidBody.h>

namespace sky::phy {

    void RigidBody::AddShape(PhysicsShape* shape)
    {
        shapes.emplace_back(shape);
    }

} // namespace sky::phy