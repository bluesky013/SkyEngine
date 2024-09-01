//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/PhysicsShape.h>
#include <physics/PhysicsMaterial.h>
#include <core/math/Vector3.h>
#include <vector>
#include <memory>

namespace sky::phy {

    enum class CollisionFlag : uint32_t {
        DYNAMIC,
        STATIC,
        KINEMATIC
    };

    class RigidBody {
    public:
        RigidBody() = default;
        virtual ~RigidBody() = default;

        void AddShape(PhysicsShape* shape);

        void SetGroup(int32_t group_) { group = group_; }
        void SetMask(int32_t mask_) { group = mask_; }

        int32_t GetGroup() const { return group; }
        int32_t GetMask() const { return mask; }

    protected:
        float   mass  = 1.f;

        int32_t group = 0;
        int32_t mask  = 0;

        CollisionFlag collisionFlag = CollisionFlag::STATIC;

        std::vector<std::unique_ptr<PhysicsShape>> shapes;
    };

} // namespace sky::phy