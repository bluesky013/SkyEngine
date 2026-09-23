//
// Created on 2026/09/22.
//

#include <vegetation/components/VegetationComponent.h>

#include <vegetation/VegetationAsset.h>
#include <vegetation/VegetationSystem.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/Actor.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

namespace sky::vegetation {

    void VegetationComponentData::Reflect(SerializationContext *context)
    {
        context->Register<VegetationComponentData>("VegetationComponentData")
            .Member<&VegetationComponentData::vegetationAsset>("vegetationAsset")
            .Member<&VegetationComponentData::source>("source")
            .Member<&VegetationComponentData::seed>("seed")
            .Member<&VegetationComponentData::pointsPerSquareMeter>("pointsPerSquareMeter")
            .Member<&VegetationComponentData::loadRadius>("loadRadius")
            .Member<&VegetationComponentData::unloadRadius>("unloadRadius")
            .Member<&VegetationComponentData::loadBudget>("loadBudget");
    }

    void VegetationComponent::Reflect(SerializationContext *context)
    {
        VegetationComponentData::Reflect(context);
        ComponentFactory::Get()->RegisterComponent<VegetationComponent>("Vegetation");
    }

    VegetationSystem *VegetationComponent::GetSystem() const
    {
        auto *world = actor != nullptr ? actor->GetWorld() : nullptr;
        if (world == nullptr) {
            return nullptr;
        }
        return static_cast<VegetationSystem *>(world->GetSubSystem(Name(VegetationSystem::NAME.data())));
    }

    void VegetationComponent::OnAttachToWorld()
    {
        auto *system = GetSystem();
        if (system == nullptr) {
            return;
        }

        if (data.vegetationAsset) {
            auto asset = AssetManager::Get()->LoadAsset<VegetationAsset>(data.vegetationAsset);
            if (asset != nullptr) {
                asset->BlockUntilLoaded();
                system->SetPalette(asset->Data().palette);
                system->SetPlacementConfig(asset->Data().config);
                system->SetInstances(asset->Data().instances);
            }
        }

        system->SetStreamingRadii(data.loadRadius, data.unloadRadius);
        system->SetLoadBudget(data.loadBudget);
        system->SetStreamingEnabled(true);

        Tick(0.f);
    }

    void VegetationComponent::OnDetachFromWorld()
    {
        if (auto *system = GetSystem()) {
            system->SetStreamingEnabled(false);
        }
    }

    void VegetationComponent::Tick(float time)
    {
        auto *system = GetSystem();
        if (system == nullptr || actor == nullptr) {
            return;
        }

        auto *transform = actor->GetComponent<TransformComponent>();
        if (transform != nullptr) {
            system->SetStreamingFocus(transform->GetWorldTransform().translation);
        }
    }

} // namespace sky::vegetation
