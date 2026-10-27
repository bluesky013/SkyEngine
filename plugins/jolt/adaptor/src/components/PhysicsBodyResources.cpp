#include "PhysicsAssets.h"
#include <jolt/components/PhysicsBodyComponent.h>

#include <cmath>
#include <framework/asset/AssetManager.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <physics/IPhysicsSystem.h>

namespace sky::phy {
    struct PhysicsBodyComponent::Resources {
        std::shared_ptr<AssetLoadRequest>      collisionRequest;
        std::shared_ptr<AssetLoadRequest>      materialRequest;
        Uuid                                   world;
        Vector3                                scale = VEC3_ONE;
        std::shared_ptr<Asset<CollisionAsset>> collision;
        std::shared_ptr<Asset<MaterialAsset>>  material;
        std::shared_ptr<const ShapeDesc>       source;
        std::shared_ptr<IPhysicsShapeRequest>  request;
    };

    PhysicsBodyComponent::PhysicsBodyComponent()  = default;
    PhysicsBodyComponent::~PhysicsBodyComponent() = default;

    void PhysicsBodyComponent::CancelPreparation()
    {
        resources.reset();
    }

    const ShapeDesc &PhysicsBodyComponent::GetResolvedShape() const
    {
        return installedShape ? *installedShape : data.shape;
    }

    ComponentPreparation PhysicsBodyComponent::PrepareForWorld(World &destination, std::string &error)
    {
        const auto fail = [&](PhysicsStatus reason) {
            status = std::move(reason);
            error  = status.message;
            return ComponentPreparation::Failed;
        };
        auto *system = static_cast<IPhysicsSystem *>(destination.GetSubSystem(Name(PHYSICS_SYSTEM_NAME.data())));
        auto *world  = system ? system->GetWorld() : nullptr;
        if (!world)
            return fail({PhysicsError::InvalidState, "Physics subsystem is unavailable"});
        const auto   *transform = actor ? actor->GetComponent<TransformComponent>() : nullptr;
        const Vector3 scale     = transform ? transform->GetWorldTransform().scale : VEC3_ONE;
        if (resources && (resources->world != destination.GetRuntimeId() || resources->scale.x != scale.x || resources->scale.y != scale.y ||
                          resources->scale.z != scale.z))
            resources.reset();
        if (!resources) {
            resources        = std::make_unique<Resources>();
            resources->world = destination.GetRuntimeId();
            resources->scale = scale;
            auto *manager    = AssetManager::Get();
            if (data.shapeSource == PhysicsShapeSource::Asset) {
                if (!data.collisionAsset)
                    return fail({PhysicsError::InvalidArgument, "Asset shape requires a Collision asset UUID"});
                resources->collisionRequest = manager->LoadAssetAsync(data.collisionAsset, AssetLoadPolicy::ProductsOnly);
            }
            if (data.materialAsset)
                resources->materialRequest = manager->LoadAssetAsync(data.materialAsset, AssetLoadPolicy::ProductsOnly);
        }
        if (resources->collisionRequest) {
            if (!resources->collisionRequest->IsResolved())
                return ComponentPreparation::Preparing;
            const auto asset = resources->collisionRequest->GetAsset();
            if (asset && asset->GetType() == Name(COLLISION_ASSET_TYPE.data()))
                resources->collision = std::static_pointer_cast<Asset<CollisionAsset>>(asset);
        }
        if (resources->materialRequest) {
            if (!resources->materialRequest->IsResolved())
                return ComponentPreparation::Preparing;
            const auto asset = resources->materialRequest->GetAsset();
            if (asset && asset->GetType() == Name(MATERIAL_ASSET_TYPE.data()))
                resources->material = std::static_pointer_cast<Asset<MaterialAsset>>(asset);
        }
        if ((data.shapeSource == PhysicsShapeSource::Asset && !resources->collision) || (data.materialAsset && !resources->material))
            return fail({PhysicsError::InvalidArgument, "Physics asset product is unavailable or has the wrong type"});
        const AssetPtr inputs[] = {resources->collision, resources->material};
        for (const auto &asset : inputs) {
            if (!asset)
                continue;
            if (asset->GetStatus() == AssetBase::Status::FAILED)
                return fail({PhysicsError::InvalidArgument, "Physics asset product failed validation"});
            if (asset->GetStatus() != AssetBase::Status::LOADED)
                return ComponentPreparation::Preparing;
        }
        if (!resources->source) {
            if (resources->collision) {
                resources->source = std::shared_ptr<const ShapeDesc>(resources->collision, &resources->collision->Data().shape);
            } else {
                std::vector<const ShapeDesc *> pending{&data.shape};
                while (!pending.empty()) {
                    const auto *shape = pending.back();
                    pending.pop_back();
                    if (shape->type == ShapeType::TriangleMesh && shape->mesh && !shape->meshData)
                        return fail({PhysicsError::Unsupported, "Streaming requires an independent Collision product for legacy render meshes"});
                    for (const auto &child : shape->children)
                        pending.push_back(&child);
                }
                resources->source = std::make_shared<const ShapeDesc>(data.shape);
            }
        }
        if (!resources->request) {
            resources->request = world->PrepareShape(resources->source, scale);
            if (!resources->request)
                return fail(world->GetLastStatus());
        }
        const auto state = resources->request->GetState();
        if (state == ShapePreparationState::Failed)
            return fail(resources->request->GetStatus());
        if (state == ShapePreparationState::Preparing)
            return ComponentPreparation::Preparing;
        PhysicsBodyDesc desc;
        desc.kind            = data.kind;
        desc.mass            = data.mass;
        desc.shape           = *resources->source;
        desc.transform.scale = scale;
        desc.linearDamping   = Vector3(data.linearDamping, data.linearDamping, data.linearDamping);
        desc.angularDamping  = Vector3(data.angularDamping, data.angularDamping, data.angularDamping);
        if (data.shapeSource > PhysicsShapeSource::Asset)
            return fail({PhysicsError::InvalidArgument, "Unknown shape source"});
        if (!ValidateBodyDesc(desc, &error))
            return fail({PhysicsError::InvalidArgument, error});
        status = {};
        return ComponentPreparation::Ready;
    }

    bool PhysicsBodyComponent::BindPrepared()
    {
        auto *world = GetPhysicsWorld();
        if (!world || !resources || !resources->request || resources->request->GetState() != ShapePreparationState::Ready)
            return false;
        PhysicsBodyDesc desc;
        desc.preparedShape  = resources->request->GetShape();
        desc.shape          = *resources->source;
        desc.kind           = data.kind;
        desc.mass           = data.mass;
        desc.filter         = data.filter;
        desc.enableCCD      = data.enableCCD;
        desc.allowSleep     = data.allowSleep;
        desc.gravityEnabled = data.gravityEnabled;
        desc.isTrigger      = data.isTrigger;
        desc.linearDamping  = Vector3(data.linearDamping, data.linearDamping, data.linearDamping);
        desc.angularDamping = Vector3(data.angularDamping, data.angularDamping, data.angularDamping);
        if (auto *transform = actor->GetComponent<TransformComponent>())
            desc.transform = transform->GetWorldTransform();
        if (resources->material) {
            desc.material = world->CreateMaterial(resources->material->Data());
            if (!IsValid(desc.material)) {
                status = world->GetLastStatus();
                return false;
            }
        }
        bool success = false;
        if (IsValid(objectId)) {
            status = world->UpdateBody(objectId, desc.kind, desc.mass, desc.shape, desc.preparedShape);
            success =
                status && world->SetBodyMaterial(objectId, desc.material) && world->SetBodyDamping(objectId, data.linearDamping, data.angularDamping);
        } else {
            objectId = world->CreateBody(desc);
            success  = IsValid(objectId);
        }
        if (!success) {
            status = world->GetLastStatus();
            if (IsValid(desc.material))
                world->DestroyMaterial(desc.material);
            return false;
        }
        if (IsValid(materialId))
            world->DestroyMaterial(materialId);
        materialId     = desc.material;
        installedShape = resources->source;
        pendingBinding = false;
        status         = {};
        return true;
    }

    void PhysicsBodyComponent::Tick(float)
    {
        if (!pendingBinding || !actor || !actor->GetWorld())
            return;
        std::string error;
        const auto  prepared = PrepareForWorld(*actor->GetWorld(), error);
        if (prepared == ComponentPreparation::Failed || (prepared == ComponentPreparation::Ready && !BindPrepared()))
            pendingBinding = false;
    }

    bool PhysicsBodyComponent::SetShapeSource(PhysicsShapeSource value)
    {
        auto next        = data;
        next.shapeSource = value;
        return Apply(next);
    }
    bool PhysicsBodyComponent::SetCollisionAsset(const Uuid &value)
    {
        auto next           = data;
        next.collisionAsset = value;
        return Apply(next);
    }
    bool PhysicsBodyComponent::SetMaterialAsset(const Uuid &value)
    {
        auto next          = data;
        next.materialAsset = value;
        return Apply(next);
    }
    bool PhysicsBodyComponent::SetLinearDamping(float value)
    {
        if (!std::isfinite(value) || value < 0)
            return false;
        if (auto *world = GetPhysicsWorld(); world && IsValid(objectId) && !world->SetBodyDamping(objectId, value, data.angularDamping))
            return false;
        data.linearDamping = value;
        return true;
    }
    bool PhysicsBodyComponent::SetAngularDamping(float value)
    {
        if (!std::isfinite(value) || value < 0)
            return false;
        if (auto *world = GetPhysicsWorld(); world && IsValid(objectId) && !world->SetBodyDamping(objectId, data.linearDamping, value))
            return false;
        data.angularDamping = value;
        return true;
    }
} // namespace sky::phy
