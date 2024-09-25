//
// Created by blues on 2024/9/25.
//

#include <physics/components/RigidBodyComponent.h>
#include <physics/PhysicsWorld.h>
#include <physics/PhysicsRegistry.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/Actor.h>
#include <framework/world/World.h>

namespace sky::phy {

    void RigidBodyComponent::Reflect(SerializationContext *context)
    {
        context->Register<CollisionFlag>("CollisionFlag")
            .Enum(CollisionFlag::DYNAMIC, "Dynamic")
            .Enum(CollisionFlag::STATIC, "Static")
            .Enum(CollisionFlag::KINEMATIC, "Kinematic");

        context->Register<RigidBodyData>("RigidBodyData")
            .Member<&RigidBodyData::mass>("mass")
            .Member<&RigidBodyData::flag>("flag");

        REGISTER_BEGIN(RigidBodyComponent, context)
            REGISTER_MEMBER(mass, SetMass, GetMass)
            REGISTER_MEMBER(flag, SetFlag, GetFlag);

        ComponentFactory::Get()->RegisterComponent<RigidBodyComponent>("Physics");
    }

    void RigidBodyComponent::SetMass(float mass)
    {
        if (rigidBody != nullptr) {
            rigidBody->SetMass(mass);
        }
        data.mass = mass;
    }

    void RigidBodyComponent::SetFlag(CollisionFlag flag)
    {
        data.flag = flag;
    }

    PhysicsWorld* RigidBodyComponent::GetWorld() const
    {
        return static_cast<PhysicsWorld*>(actor->GetWorld()->GetSubSystem(PhysicsWorld::NAME.data()));
    }

    void RigidBodyComponent::SetRigidBody()
    {
        rigidBody->SetMass(data.mass);
    }

    void RigidBodyComponent::OnAttachToWorld()
    {
        auto *world = GetWorld();
        if (world != nullptr) {
            rigidBody = PhysicsRegistry::Get()->CreateRigidBody();
            SetRigidBody();
            world->AddRigidBody(rigidBody);
        }
    }

    void RigidBodyComponent::OnDetachFromWorld()
    {
        auto *world = GetWorld();
        if (world != nullptr && rigidBody != nullptr) {
            world->RemoveRigidBody(rigidBody);
        }
    }

    void RigidBodyComponent::Tick(float time)
    {

    }
} // namespace sky::phy
