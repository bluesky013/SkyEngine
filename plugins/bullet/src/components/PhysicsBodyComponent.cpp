//
// Created on 2026/09/23.
//

#include <bullet/components/PhysicsBodyComponent.h>
#include <bullet/PhysicsSystem.h>

#include <physics/IPhysicsBackend.h>
#include <physics/PhysicsMesh.h>

#include <framework/world/Actor.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

namespace sky::phy {

    void PhysicsBodyComponent::Reflect(SerializationContext *context)
    {
        context->Register<BodyKind>("PhysicsBodyKind")
            .Enum(BodyKind::Static, "Static")
            .Enum(BodyKind::Dynamic, "Dynamic")
            .Enum(BodyKind::Kinematic, "Kinematic");

        context->Register<ShapeType>("PhysicsShapeType")
            .Enum(ShapeType::Box, "Box")
            .Enum(ShapeType::Sphere, "Sphere")
            .Enum(ShapeType::Capsule, "Capsule")
            .Enum(ShapeType::HeightField, "HeightField")
            .Enum(ShapeType::TriangleMesh, "TriangleMesh")
            .Enum(ShapeType::ConvexHull, "ConvexHull")
            .Enum(ShapeType::Compound, "Compound");

        context->Register<CollisionFilter>("PhysicsCollisionFilter")
            .Member<&CollisionFilter::group>("group")
            .Member<&CollisionFilter::mask>("mask");

        context->Register<ShapeDesc>("PhysicsShapeDesc")
            .Member<&ShapeDesc::type>("type")
            .Member<&ShapeDesc::pivot>("pivot")
            .Member<&ShapeDesc::halfExt>("halfExt")
            .Member<&ShapeDesc::radius>("radius")
            .Member<&ShapeDesc::height>("height")
            .Member<&ShapeDesc::cols>("cols")
            .Member<&ShapeDesc::rows>("rows")
            .Member<&ShapeDesc::scaleX>("scaleX")
            .Member<&ShapeDesc::scaleZ>("scaleZ")
            .Member<&ShapeDesc::heightScale>("heightScale")
            .Member<&ShapeDesc::heightOffset>("heightOffset")
            .Member<&ShapeDesc::minHeight>("minHeight")
            .Member<&ShapeDesc::maxHeight>("maxHeight")
            .Member<&ShapeDesc::upAxis>("upAxis")
            .Member<&ShapeDesc::mesh>("mesh");

        context->Register<PhysicsBodyData>("PhysicsBodyData")
            .Member<&PhysicsBodyData::mass>("mass")
            .Member<&PhysicsBodyData::kind>("kind")
            .Member<&PhysicsBodyData::enableCCD>("enableCCD")
            .Member<&PhysicsBodyData::gravityEnabled>("gravityEnabled")
            .Member<&PhysicsBodyData::allowSleep>("allowSleep")
            .Member<&PhysicsBodyData::isTrigger>("isTrigger")
            .Member<&PhysicsBodyData::shape>("shape")
            .Member<&PhysicsBodyData::filter>("filter");

        REGISTER_BEGIN(PhysicsBodyComponent, context)
            REGISTER_MEMBER(mass, SetMass, GetMass)
            REGISTER_MEMBER(kind, SetKind, GetKind)
            REGISTER_MEMBER_NS(shape, GetShape, ShapeChanged);

        ComponentFactory::Get()->RegisterComponent<PhysicsBodyComponent>("PhysicsBody");
    }

    IPhysicsWorld *PhysicsBodyComponent::GetPhysicsWorld() const
    {
        if (actor == nullptr || actor->GetWorld() == nullptr) {
            return nullptr;
        }
        auto *system = static_cast<PhysicsSystem *>(
            actor->GetWorld()->GetSubSystem(Name(PhysicsSystem::NAME.data())));
        return system != nullptr ? system->GetWorld() : nullptr;
    }

    void PhysicsBodyComponent::CreateBody()
    {
        IPhysicsWorld *world = GetPhysicsWorld();
        if (world == nullptr) {
            return;
        }

        if (data.shape.type == ShapeType::TriangleMesh && !data.shape.meshData &&
            static_cast<bool>(data.shape.mesh)) {
            data.shape.meshData = CreatePhysicsMesh(data.shape.mesh);
        }

        PhysicsBodyDesc desc;
        desc.kind           = data.kind;
        desc.shape          = data.shape;
        desc.mass           = data.mass;
        desc.filter         = data.filter;
        desc.enableCCD      = data.enableCCD;
        desc.gravityEnabled = data.gravityEnabled;
        desc.allowSleep     = data.allowSleep;
        desc.isTrigger      = data.isTrigger;

        if (auto *transform = actor->GetComponent<TransformComponent>()) {
            desc.transform = transform->GetWorldTransform();
        }

        objectId = world->CreateBody(desc);
    }

    void PhysicsBodyComponent::DestroyBody()
    {
        if (objectId == INVALID_PHYSICS_OBJECT_ID) {
            return;
        }
        if (IPhysicsWorld *world = GetPhysicsWorld()) {
            world->DestroyObject(objectId);
        }
        objectId = INVALID_PHYSICS_OBJECT_ID;
    }

    void PhysicsBodyComponent::Recreate()
    {
        DestroyBody();
        CreateBody();
    }

    void PhysicsBodyComponent::OnAttachToWorld()
    {
        if (objectId == INVALID_PHYSICS_OBJECT_ID) {
            CreateBody();
        }
    }

    void PhysicsBodyComponent::OnDetachFromWorld()
    {
        DestroyBody();
    }

    void PhysicsBodyComponent::Tick(float time)
    {
        if (objectId == INVALID_PHYSICS_OBJECT_ID || data.kind == BodyKind::Static) {
            return;
        }
        IPhysicsWorld *world = GetPhysicsWorld();
        if (world == nullptr) {
            return;
        }
        Transform transform;
        if (world->GetInterpolatedTransform(objectId, transform)) {
            if (auto *transformComponent = actor->GetComponent<TransformComponent>()) {
                transformComponent->SetWorldTransform(transform);
            }
        }
    }

    void PhysicsBodyComponent::SetMass(float mass)
    {
        data.mass = mass;
        Recreate();
    }

    void PhysicsBodyComponent::SetKind(BodyKind kind)
    {
        data.kind = kind;
        Recreate();
    }

    void PhysicsBodyComponent::SetShape(const ShapeDesc &shape)
    {
        data.shape = shape;
        ShapeChanged();
    }

    void PhysicsBodyComponent::ShapeChanged()
    {
        Recreate();
    }

} // namespace sky::phy
