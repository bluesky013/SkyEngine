//
// Created by blues on 2024/9/25.
//

#pragma once

#include <framework/world/Component.h>
#include <framework/serialization/ArrayVisitor.h>
#include <physics/RigidBody.h>
#include <physics/PhysicsBase.h>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::phy {

    class PhysicsWorld;

    struct CollisionData {
        MeshPhysicsConfig config;
    };

    class CollisionComponent : public ComponentAdaptor<CollisionData> {
    public:
        CollisionComponent() = default;
        ~CollisionComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(CollisionComponent)

        SequenceVisitor Spheres();
        SequenceVisitor Boxes();
        void ShapeChanged();

    private:
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
        void Tick(float time) override;

        void RebuildShape();
        PhysicsWorld* GetWorld() const;

        PhysicsShape* shape = nullptr;
        CollisionObject* collisionObject = nullptr;
    };

} // namespace sky::phy

