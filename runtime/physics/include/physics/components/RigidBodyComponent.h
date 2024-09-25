//
// Created by blues on 2024/9/25.
//

#pragma once

#include <framework/world/Component.h>
#include <physics/RigidBody.h>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::phy {

    class PhysicsWorld;

    struct RigidBodyData {
        float mass = 1.f;
        CollisionFlag flag = CollisionFlag::STATIC;
    };

    class RigidBodyComponent : public ComponentAdaptor<RigidBodyData> {
    public:
        RigidBodyComponent() = default;
        ~RigidBodyComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(RigidBodyComponent)

        void SetMass(float mass);
        float GetMass() const { return data.mass; }

        void SetFlag(CollisionFlag flag);
        CollisionFlag GetFlag() const { return data.flag; }

    private:
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
        void Tick(float time) override;

        PhysicsWorld* GetWorld() const;
        void SetRigidBody();

        RigidBody* rigidBody = nullptr;
    };

} // namespace sky::phy
