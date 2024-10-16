//
// Created by blues on 2024/9/25.
//

#include <physics/components/RigidBodyComponent.h>
#include <physics/PhysicsWorld.h>
#include <physics/PhysicsRegistry.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/Actor.h>
#include <framework/world/World.h>
#include <framework/asset/AssetManager.h>

namespace sky::phy {

    void RigidBodyComponent::Reflect(SerializationContext *context)
    {
        context->Register<CollisionFlag>("CollisionFlag")
            .Enum(CollisionFlag::DYNAMIC, "Dynamic")
            .Enum(CollisionFlag::STATIC, "Static")
            .Enum(CollisionFlag::KINEMATIC, "Kinematic");

        context->Register<RigidBodyData>("RigidBodyData")
            .Member<&RigidBodyData::mass>("mass")
            .Member<&RigidBodyData::flag>("flag")
            .Member<&RigidBodyData::config>("config");

        REGISTER_BEGIN(RigidBodyComponent, context)
            REGISTER_MEMBER(mass, SetMass, GetMass)
            REGISTER_MEMBER(flag, SetFlag, GetFlag)
            REGISTER_MEMBER(triangleMesh, SetTriangleMesh, GetTriangleMesh)
                SET_ASSET_TYPE(std::string_view("Mesh"))
            REGISTER_MEMBER_NS(shapeSpheres, Spheres, ShapeChanged)
            REGISTER_MEMBER_NS(shapeBoxes, Boxes, ShapeChanged);

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

    void RigidBodyComponent::SetTriangleMesh(const Uuid &mesh)
    {
        data.config.tris.asset = mesh;
        ShapeChanged();
    }

    void RigidBodyComponent::ShapeChanged()
    {
        RebuildShape();
        if (rigidBody != nullptr) {
            rigidBody->SetShape(shape);
        }
    }

    SequenceVisitor RigidBodyComponent::Spheres()
    {
        const auto *info = TypeInfoObj<std::vector<SphereShape>>::Get()->RtInfo();
        SKY_ASSERT(info != nullptr);

        return {info->containerInfo, reinterpret_cast<void*>(&data.config.sphere)};
    }

    SequenceVisitor RigidBodyComponent::Boxes()
    {
        const auto *info = TypeInfoObj<std::vector<BoxShape>>::Get()->RtInfo();
        SKY_ASSERT(info != nullptr);

        return {info->containerInfo, reinterpret_cast<void*>(&data.config.box)};
    }

    PhysicsWorld* RigidBodyComponent::GetWorld() const
    {
        return static_cast<PhysicsWorld*>(actor->GetWorld()->GetSubSystem(PhysicsWorld::NAME.data()));
    }

    void RigidBodyComponent::SetupRigidBody()
    {
        rigidBody->SetStartTrans(actor->GetComponent<TransformComponent>()->GetWorldTransform());
        rigidBody->SetMass(data.mass);
        rigidBody->SetShape(shape);
        rigidBody->SetFlag(data.flag);
    }

    void RigidBodyComponent::RebuildShape()
    {
        // TODO
        if (data.config.tris.asset) {
            shape = new PhysicsTriangleMeshShape(data.config.tris);
        } else if (!data.config.sphere.empty()) {
            const auto &sphere = data.config.sphere[0];
            shape = new PhysicsSphereShape(sphere);
        } else if (!data.config.box.empty()) {
            const auto &box = data.config.box[0];
            shape = new PhysicsBoxShape(box);
        } else {
            shape = nullptr;
        }
    }

    void RigidBodyComponent::OnAttachToWorld()
    {
        auto *world = GetWorld();
        if (world != nullptr) {
            RebuildShape();

            rigidBody = PhysicsRegistry::Get()->CreateRigidBody();
            rigidBody->SetMotionCallBack(this);
            world->AddRigidBody(rigidBody);
            SetupRigidBody();
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

    void RigidBodyComponent::OnRigidBodyUpdate(const Transform &trans)
    {
        actor->GetComponent<TransformComponent>()->SetWorldTransform(trans);
    }
} // namespace sky::phy
