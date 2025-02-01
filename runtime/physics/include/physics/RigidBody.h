//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/PhysicsMaterial.h>
#include <physics/CollisionObject.h>
#include <core/math/Vector3.h>
#include <core/math/Transform.h>
#include <vector>
#include <memory>

namespace sky::phy {

    class IMotionCallBack {
    public:
        IMotionCallBack() = default;
        virtual ~IMotionCallBack() = default;

        virtual void OnRigidBodyUpdate(const Transform &trans) = 0;
    };

    enum class CollisionFlag : uint32_t {
        DYNAMIC,
        STATIC,
        KINEMATIC
    };

    class RigidBody {
    public:
        RigidBody();
        virtual ~RigidBody() = default;

        void SetShape(PhysicsShape *shape);
        void SetGroup(CollisionFilters group_);
        void SetMask(CollisionFilters mask_);
        void SetStartTrans(const Transform &trans);
        void SetMotionCallBack(IMotionCallBack* callback) { listener = callback; }

        virtual void SetMass(float m) = 0;
        virtual void SetFlag(CollisionFlag m) = 0;

    protected:
        virtual void OnShapeChanged() = 0;
        virtual void OnGroupMaskChanged() = 0;

        CollisionFilters group = CollisionFilterBit::ALL;
        CollisionFilters mask = CollisionFilterBit::ALL;
        std::unique_ptr<PhysicsShape> physicsShape;

        float mass  = 1.f;
        CollisionFlag collisionFlag = CollisionFlag::DYNAMIC;
        Transform startTrans = Transform::GetIdentity();

        IMotionCallBack* listener = nullptr;
    };

} // namespace sky::phy