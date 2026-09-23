//
// Created on 2026/09/22.
//

#include <terrain/components/TerrainComponent.h>

#include <terrain/TerrainAsset.h>
#include <terrain/TerrainGenerator.h>
#include <terrain/TerrainSystem.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/Actor.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

namespace sky::terrain {

    TerrainMeta TerrainComponentData::ToMeta() const
    {
        TerrainMeta meta;
        meta.tileSize     = tileSize;
        meta.resolution   = resolution;
        meta.heightFormat = static_cast<TerrainHeightFormat>(heightFormat);
        meta.heightScale  = heightScale;
        meta.heightOffset = heightOffset;
        meta.tileCountX   = tileCountX;
        meta.tileCountY   = tileCountY;
        meta.layerCount   = layerCount;
        meta.lodCount     = lodCount > 0 ? lodCount : 1u;
        meta.origin       = Vector3(originX, originY, originZ);
        return meta;
    }

    void TerrainComponentData::Reflect(SerializationContext *context)
    {
        context->Register<TerrainComponentData>("TerrainComponentData")
            .Member<&TerrainComponentData::tileSize>("tileSize")
            .Member<&TerrainComponentData::resolution>("resolution")
            .Member<&TerrainComponentData::heightFormat>("heightFormat")
            .Member<&TerrainComponentData::heightScale>("heightScale")
            .Member<&TerrainComponentData::heightOffset>("heightOffset")
            .Member<&TerrainComponentData::tileCountX>("tileCountX")
            .Member<&TerrainComponentData::tileCountY>("tileCountY")
            .Member<&TerrainComponentData::layerCount>("layerCount")
            .Member<&TerrainComponentData::lodCount>("lodCount")
            .Member<&TerrainComponentData::originX>("originX")
            .Member<&TerrainComponentData::originY>("originY")
            .Member<&TerrainComponentData::originZ>("originZ")
            .Member<&TerrainComponentData::terrainAsset>("terrainAsset")
            .Member<&TerrainComponentData::source>("source")
            .Member<&TerrainComponentData::material>("material")
            .Member<&TerrainComponentData::loadRadius>("loadRadius")
            .Member<&TerrainComponentData::unloadRadius>("unloadRadius")
            .Member<&TerrainComponentData::loadBudget>("loadBudget")
            .Member<&TerrainComponentData::generateSeed>("generateSeed")
            .Member<&TerrainComponentData::generationEnabled>("generationEnabled");
    }

    void TerrainComponent::Reflect(SerializationContext *context)
    {
        TerrainComponentData::Reflect(context);
        ComponentFactory::Get()->RegisterComponent<TerrainComponent>("Terrain");
    }

    TerrainSystem *TerrainComponent::GetSystem() const
    {
        auto *world = actor != nullptr ? actor->GetWorld() : nullptr;
        if (world == nullptr) {
            return nullptr;
        }
        return static_cast<TerrainSystem *>(world->GetSubSystem(Name(TerrainSystem::NAME.data())));
    }

    void TerrainComponent::OnAttachToWorld()
    {
        auto *system = GetSystem();
        if (system == nullptr) {
            return;
        }

        TerrainAssetData assetData;
        assetData.meta = data.ToMeta();
        if (data.terrainAsset) {
            auto asset = AssetManager::Get()->LoadAsset<TerrainAsset>(data.terrainAsset);
            if (asset != nullptr) {
                asset->BlockUntilLoaded();
                assetData = asset->Data();
            }
        } else if (data.generationEnabled == 0) {
            // No cooked asset and no generation: nothing to page from.
            return;
        }

        system->Setup(assetData);
        system->SetStreamingRadii(data.loadRadius, data.unloadRadius);
        system->SetLoadBudget(data.loadBudget);
        system->SetStreamingEnabled(true);

        if (data.generationEnabled != 0) {
            TerrainGenerateConfig config;
            config.seed         = data.generateSeed;
            config.lodCount     = data.lodCount;
            config.heightScale  = data.heightScale;
            config.heightOffset = data.heightOffset;
            config.layerCount   = data.layerCount;
            system->SetGenerateConfig(config);
            system->SetGenerationEnabled(true);
        }

        Tick(0.f);
    }

    void TerrainComponent::OnDetachFromWorld()
    {
        if (auto *system = GetSystem()) {
            system->SetStreamingEnabled(false);
            system->OnTerrainChanged();
        }
    }

    void TerrainComponent::Tick(float time)
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

} // namespace sky::terrain
