//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/SerializationFactory.h>
#include <framework/world/World.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <physics/PhysicsBackendRegistry.h>

#include <bullet/BulletBackend.h>
#include <bullet/PhysicsSubSystemConfig.h>
#include <bullet/PhysicsSystem.h>
#include <bullet/components/PhysicsBodyComponent.h>

#include <memory>
#include <string>

namespace sky::phy {

    class BulletPhysicsModule : public IModule {
    public:
        BulletPhysicsModule()           = default;
        ~BulletPhysicsModule() override = default;

        bool Init(const StartArguments &args) override
        {
            return true;
        }

        void Start() override
        {
            PhysicsBackendRegistry::Get().Register(std::make_unique<BulletBackend>());
            PhysicsBackendRegistry::Get().SetWorldAttacher([](World &world) -> IWorldSubSystem * { return AttachPhysicsSystem(world); });

            PhysicsBodyComponent::Reflect(SerializationContext::Get());
            PhysicsSubSystemConfig::Reflect(SerializationContext::Get());

            // Declarative world subsystem: create a PhysicsSystem from the config.
            WorldSubSystemRegistry::Get().Register(Name(PhysicsSystem::NAME.data()),
                                                   WorldSubSystemRegistration{
                                                       [](World &, const Any &config) -> std::unique_ptr<IWorldSubSystem> {
                                                           auto system = std::make_unique<PhysicsSystem>();
                                                           if (!system->Init()) {
                                                               return nullptr;
                                                           }
                                                           (void)config; // config -> PhysicsWorldDesc mapping is a follow-up
                                                           return system;
                                                       },
                                                       TypeInfoObj<PhysicsSubSystemConfig>::Get()->RtInfo(),
                                                       [] { return Any(std::in_place_type<PhysicsSubSystemConfig>); },
                                                       [](const Any &, std::string &) { return true; },
                                                   });
        }

        void Shutdown() override
        {
            WorldSubSystemRegistry::Get().Unregister(Name(PhysicsSystem::NAME.data()));
            PhysicsBackendRegistry::Get().SetWorldAttacher(nullptr);
            PhysicsBackendRegistry::Get().Unregister();
        }
    };

} // namespace sky::phy
REGISTER_MODULE(sky::phy::BulletPhysicsModule)
