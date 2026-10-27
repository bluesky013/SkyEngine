//
// Created on 2026/09/23.
//

#include <cmath>
#include <core/logger/Logger.h>
#include <jolt/components/PhysicsBodyComponent.h>
#include <physics/IPhysicsSystem.h>

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

        context->Register<CollisionFilter>("PhysicsCollisionFilter").Member<&CollisionFilter::group>("group").Member<&CollisionFilter::mask>("mask");

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
            .Member<&ShapeDesc::mesh>("mesh")
            .Member<&ShapeDesc::samples>("samples")
            .Member<&ShapeDesc::points>("points")
            .Member<&ShapeDesc::children>("children");

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
        REGISTER_MEMBER(shape, SetShape, GetShape);

        ComponentFactory::Get()->RegisterComponent<PhysicsBodyComponent>("PhysicsBody", {Name(PHYSICS_SYSTEM_NAME.data())});
    }

    IPhysicsWorld *PhysicsBodyComponent::GetPhysicsWorld() const
    {
        if (actor == nullptr || actor->GetWorld() == nullptr) {
            return nullptr;
        }
        auto *system = static_cast<IPhysicsSystem *>(actor->GetWorld()->GetSubSystem(Name(PHYSICS_SYSTEM_NAME.data())));
        return system != nullptr ? system->GetWorld() : nullptr;
    }

    bool PhysicsBodyComponent::Apply(const PhysicsBodyData &candidate)
    {
        if (!std::isfinite(candidate.mass) || candidate.mass <= 0 || candidate.kind > BodyKind::Kinematic) {
            status = {PhysicsError::InvalidArgument, "Invalid body authoring properties"};
            return false;
        }
        auto            next = candidate;
        PhysicsBodyDesc desc;
        desc.kind           = next.kind;
        desc.shape          = next.shape;
        desc.mass           = next.mass;
        desc.filter         = next.filter;
        desc.enableCCD      = next.enableCCD;
        desc.gravityEnabled = next.gravityEnabled;
        desc.allowSleep     = next.allowSleep;
        desc.isTrigger      = next.isTrigger;
        if (actor) {
            if (auto *transform = actor->GetComponent<TransformComponent>())
                desc.transform = transform->GetWorldTransform();
        }
        std::string error;
        if (!ValidateBodyDesc(desc, &error)) {
            status = {PhysicsError::InvalidArgument, std::move(error)};
            return false;
        }
        auto *world = GetPhysicsWorld();
        if (world) {
            std::vector<ShapeDesc *> pending{&next.shape};
            while (!pending.empty()) {
                auto *shape = pending.back();
                pending.pop_back();
                if (shape->type == ShapeType::TriangleMesh && shape->mesh) {
                    shape->meshData = CreatePhysicsMesh(shape->mesh);
                    if (!shape->meshData) {
                        status = {PhysicsError::InvalidArgument, "Collision mesh product is unavailable or invalid"};
                        return false;
                    }
                }
                if (shape->type == ShapeType::Compound)
                    for (auto &child : shape->children)
                        pending.push_back(&child);
            }
            desc.shape = next.shape;
            if (IsValid(objectId)) {
                status = world->UpdateBody(objectId, next.kind, next.mass, next.shape);
                if (!status)
                    return false;
            } else {
                const auto created = world->CreateBody(desc);
                status             = world->GetLastStatus();
                if (!IsValid(created))
                    return false;
                objectId = created;
            }
        }
        data   = std::move(next);
        status = {};
        return true;
    }

    void PhysicsBodyComponent::DestroyBody()
    {
        if (objectId == INVALID_PHYSICS_OBJECT_ID) {
            return;
        }
        if (IPhysicsWorld *world = GetPhysicsWorld()) {
            if (world->HasObject(objectId))
                world->DestroyObject(objectId);
        }
        objectId = INVALID_PHYSICS_OBJECT_ID;
    }

    bool PhysicsBodyComponent::TryAttachToWorld(std::string &error)
    {
        TransformEvent::Connect(actor, this);
        if (!Apply(data)) {
            error = status.message;
            return false;
        }
        return true;
    }

    void PhysicsBodyComponent::OnDetachFromWorld()
    {
        TransformEvent::DisConnect(actor, this);
        DestroyBody();
    }

    void PhysicsBodyComponent::PostTick(float time)
    {
        if (objectId == INVALID_PHYSICS_OBJECT_ID || data.kind != BodyKind::Dynamic) {
            return;
        }
        IPhysicsWorld *world = GetPhysicsWorld();
        if (world == nullptr) {
            return;
        }
        Transform transform;
        if (world->GetInterpolatedTransform(objectId, transform)) {
            if (auto *transformComponent = actor->GetComponent<TransformComponent>()) {
                writingTransform = true;
                transformComponent->SetWorldTransform(transform);
                writingTransform = false;
            }
        }
    }

    bool PhysicsBodyComponent::SetMass(float mass)
    {
        auto next = data;
        next.mass = mass;
        return Apply(next);
    }

    bool PhysicsBodyComponent::SetKind(BodyKind kind)
    {
        auto next = data;
        next.kind = kind;
        return Apply(next);
    }

    bool PhysicsBodyComponent::SetShape(const ShapeDesc &shape)
    {
        auto next  = data;
        next.shape = shape;
        return Apply(next);
    }

    void PhysicsBodyComponent::OnTransformChanged(const Transform &global, const Transform &local)
    {
        if (writingTransform || !IsValid(objectId))
            return;
        auto *world = GetPhysicsWorld();
        if (!world)
            return;
        const bool updated =
            data.kind == BodyKind::Kinematic ? world->SetKinematicTarget(objectId, global) : world->SetBodyTransform(objectId, global);
        status = updated ? PhysicsStatus{} : world->GetLastStatus();
        if (!updated) {
            LOG_E("Jolt", "%s", status.message.c_str());
            Transform previous;
            if (world->GetBodyTransform(objectId, previous)) {
                writingTransform = true;
                actor->GetComponent<TransformComponent>()->SetWorldTransform(previous);
                writingTransform = false;
            }
        }
    }

    void PhysicsBodyComponent::CollectAssetReferences(std::vector<ComponentAssetReference> &out) const
    {
        const auto                     type = GetPhysicsMeshAssetType();
        std::vector<const ShapeDesc *> pending{&data.shape};
        while (!pending.empty()) {
            const auto *shape = pending.back();
            pending.pop_back();
            if (shape->type == ShapeType::TriangleMesh && shape->mesh)
                out.push_back({shape->mesh, type});
            if (shape->type == ShapeType::Compound)
                for (const auto &child : shape->children)
                    pending.push_back(&child);
        }
    }

} // namespace sky::phy
