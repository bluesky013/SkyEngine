//
// Created by blues on 2024/11/7.
//

#include <terrain/TerrainModule.h>
#include <framework/world/ComponentFactory.h>
#include <framework/serialization/SerializationContext.h>
#include <terrain/TerrainComponent.h>

namespace sky {

    bool TerrainModule::Init(const StartArguments &args)
    {
        auto *context = SerializationContext::Get();

        TerrainComponent::Reflect(context);
        ComponentFactory::Get()->RegisterComponent<TerrainComponent>("Terrain");
        return true;
    }

} // namespace sky