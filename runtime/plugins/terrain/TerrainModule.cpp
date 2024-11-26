//
// Created by blues on 2024/11/7.
//

#include <framework/interface/IModule.h>
#include <framework/world/Component.h>
#include <framework/world/ComponentFactory.h>
#include <framework/serialization/SerializationContext.h>
#include <terrain/TerrainComponent.h>

namespace sky {

    class TerrainModule : public IModule {
    public:
        TerrainModule() = default;
        ~TerrainModule() override = default;

        bool Init(const StartArguments &args) override
        {
            auto *context = SerializationContext::Get();

            TerrainComponent::Reflect(context);
            ComponentFactory::Get()->RegisterComponent<TerrainComponent>("Terrain");
            return true;
        }

        void Start() override
        {
        }

        void Shutdown() override
        {
        }
    };

} // namespace sky

REGISTER_MODULE(sky::TerrainModule)