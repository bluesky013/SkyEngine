//
// Created by blues on 2024/10/6.
//

#include <physics/components/CollisionComponent.h>
#include <physics/PhysicsWorld.h>
#include <physics/PhysicsRegistry.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>

namespace sky::phy {

    void CollisionComponent::Reflect(SerializationContext *context)
    {
        context->Register<CollisionData>("CollisionData")
                .Member<&CollisionData::config>("config");

        REGISTER_BEGIN(CollisionComponent, context)
                REGISTER_MEMBER_NS(shapeSpheres, Spheres, ShapeChanged)
                REGISTER_MEMBER_NS(shapeBoxes, Boxes, ShapeChanged)
                REGISTER_MEMBER(triangleMesh, SetTriangleMesh, GetTriangleMesh)
                    SET_ASSET_TYPE(std::string_view("Mesh"));

        ComponentFactory::Get()->RegisterComponent<CollisionComponent>("Physics");
    }

    void CollisionComponent::SetTriangleMesh(const Uuid &mesh)
    {
        data.config.tris.asset = mesh;
    }

    SequenceVisitor CollisionComponent::Spheres()
    {
        const auto *info = TypeInfoObj<std::vector<SphereShape>>::Get()->RtInfo();
        SKY_ASSERT(info != nullptr);

        return {info->containerInfo, reinterpret_cast<void*>(&data.config.sphere)};
    }

    SequenceVisitor CollisionComponent::Boxes()
    {
        const auto *info = TypeInfoObj<std::vector<BoxShape>>::Get()->RtInfo();
        SKY_ASSERT(info != nullptr);

        return {info->containerInfo, reinterpret_cast<void*>(&data.config.box)};
    }

    void CollisionComponent::ShapeChanged()
    {
        RebuildShape();
        if (collisionObject != nullptr) {
            collisionObject->SetShape(shape);
        }
    }

    void CollisionComponent::OnAttachToWorld()
    {
        auto *world = GetWorld();
        if (world != nullptr) {
            RebuildShape();

            collisionObject = PhysicsRegistry::Get()->CreateCollisionObject();
            collisionObject->SetShape(shape);
            collisionObject->SetWorldTransform(actor->GetComponent<TransformComponent>()->GetWorldTransform());
            world->AddCollisionObject(collisionObject);
        }
    }

    void CollisionComponent::OnDetachFromWorld()
    {
        auto *world = GetWorld();
        if (world != nullptr && collisionObject != nullptr) {
            world->RemoveCollisionObject(collisionObject);
        }
    }

    void CollisionComponent::Tick(float time)
    {

    }

    void CollisionComponent::RebuildShape()
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

    PhysicsWorld* CollisionComponent::GetWorld() const
    {
        return static_cast<PhysicsWorld*>(actor->GetWorld()->GetSubSystem(PhysicsWorld::NAME.data()));
    }
} // namespace sky::phy