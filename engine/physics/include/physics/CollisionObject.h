//
// Created by blues on 2024/10/6.
//

#pragma once

#include <core/math/Transform.h>
#include <physics/PhysicsShape.h>
#include <physics/PhysicsMaterial.h>

namespace sky::phy {
    class PhysicsWorld;

    class CollisionObject {
    public:
        CollisionObject() = default;
        virtual ~CollisionObject() = default;

        virtual void SetWorldTransform(const Transform &trans) = 0;
        virtual PhysicsWorld* GetWorld() const = 0;

        void SetShape(PhysicsShape *shape);
        void SetGroup(const CollisionFilters& group_);
        void SetMask(const CollisionFilters& mask_);

        void SetMaterial(PhysicsMaterial *mat)
        {
            material = mat;
            OnMaterialChanged();
        }
        PhysicsMaterial *GetMaterial() const { return material; }

        CollisionFilters GetGroup() const { return group; }
        CollisionFilters GetMask() const { return mask; }

    protected:
        virtual void OnShapeChanged() = 0;
        virtual void OnGroupMaskChanged() = 0;
        virtual void OnMaterialChanged() {}

        CollisionFilters group = CollisionFilterBit::ALL;
        CollisionFilters mask = CollisionFilterBit::ALL;
        std::unique_ptr<PhysicsShape> physicsShape;
        PhysicsMaterial *material = nullptr;
    };

} // namespace sky::phy
