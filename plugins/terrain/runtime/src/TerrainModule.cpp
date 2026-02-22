//
// Created by blues on 2024/11/7.
//

#include <terrain/TerrainModule.h>
#include <framework/world/ComponentFactory.h>
#include <framework/serialization/SerializationContext.h>
#include <terrain/TerrainComponent.h>
#include <terrain/TerrainFeature.h>

namespace sky {

    bool TerrainModule::Init(const StartArguments &args)
    {
        auto *context = SerializationContext::Get();

        TerrainComponent::Reflect(context);
        ComponentFactory::Get()->RegisterComponent<TerrainComponent>("Terrain");

        TerrainFeature::Get()->Init();
        return true;
    }

    void TerrainModule::Shutdown()
    {
        TerrainFeature::Destroy();
    }

} // namespace sky