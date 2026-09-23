//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <framework/world/World.h>
#include <framework/serialization/SerializationContext.h>

#include <physics/PhysicsBackendRegistry.h>

#include <bullet/BulletBackend.h>
#include <bullet/PhysicsSystem.h>
#include <bullet/components/PhysicsBodyComponent.h>

#include <memory>

namespace sky::phy {

    class BulletPhysicsModule : public IModule {
    public:
        BulletPhysicsModule() = default;
        ~BulletPhysicsModule() override = default;

        bool Init(const StartArguments &args) override
        {
            return true;
        }

        void Start() override
        {
            PhysicsBackendRegistry::Get().Register(std::make_unique<BulletBackend>());
            PhysicsBackendRegistry::Get().SetWorldAttacher(
                [](World &world) -> IWorldSubSystem * { return AttachPhysicsSystem(world); });

            PhysicsBodyComponent::Reflect(SerializationContext::Get());
        }

        void Shutdown() override
        {
            PhysicsBackendRegistry::Get().SetWorldAttacher(nullptr);
            PhysicsBackendRegistry::Get().Unregister();
        }
    };

} // namespace sky::phy
REGISTER_MODULE(sky::phy::BulletPhysicsModule)
