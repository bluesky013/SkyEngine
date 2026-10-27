//
// Created on 2026/10/26.
//

#include "JoltBackend.h"
#include <core/logger/Logger.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/SerializationFactory.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <jolt/PhysicsSubSystemConfig.h>
#include <jolt/PhysicsSystem.h>
#include <jolt/components/PhysicsBodyComponent.h>
#include <physics/PhysicsBackendRegistry.h>

namespace sky::phy {
    static PhysicsStatus ReadConfig(const Any &config, PhysicsWorldDesc &world, PhysicsStepConfig &step)
    {
        if (!config) {
            return PhysicsSubSystemConfig{}.MakeSettings(world, step);
        }
        const auto *value = config.GetAsConst<PhysicsSubSystemConfig>();
        return value ? value->MakeSettings(world, step) : PhysicsStatus{PhysicsError::InvalidArgument, "Unexpected Physics config type"};
    }
    class JoltPhysicsModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            PhysicsBodyComponent::Reflect(SerializationContext::Get());
            PhysicsSubSystemConfig::Reflect(SerializationContext::Get());
        }
        bool Init(const StartArguments &) override
        {
            auto *manager = ModuleManager::Current();
            lease         = manager ? manager->RetainModule("JoltPhysicsModule") : nullptr;
            backend       = std::make_unique<JoltBackend>(lease);
            if (!backend->Init()) {
                LOG_E("Jolt", "%s", backend->GetLastStatus().message.c_str());
                backend.reset();
                return false;
            }
            return true;
        }
        void Start() override
        {
            if (!PhysicsBackendRegistry::Get().Register(std::move(backend))) {
                LOG_E("Jolt", "Backend registration failed");
                return;
            }
            started = true;
            WorldSubSystemRegistration registration;
            registration.factory = [](World &, const Any &config) -> std::unique_ptr<IWorldSubSystem> {
                PhysicsWorldDesc  desc;
                PhysicsStepConfig step;
                auto              status = ReadConfig(config, desc, step);
                if (!status) {
                    LOG_E("Jolt", "%s", status.message.c_str());
                    return nullptr;
                }
                auto system = std::make_unique<PhysicsSystem>();
                if (!system->Init(desc)) {
                    LOG_E("Jolt", "%s", system->GetLastStatus().message.c_str());
                    return nullptr;
                }
                system->SetStepConfig(step);
                return system;
            };
            registration.configType        = TypeInfoObj<PhysicsSubSystemConfig>::Get()->RtInfo();
            registration.makeDefaultConfig = [] { return Any(std::in_place_type<PhysicsSubSystemConfig>); };
            registration.validate          = [](const Any &config, std::string &error) {
                PhysicsWorldDesc  desc;
                PhysicsStepConfig step;
                auto              status = ReadConfig(config, desc, step);
                error                    = status.message;
                return static_cast<bool>(status);
            };
            registration.moduleLease = lease;
            worldRegistered          = WorldSubSystemRegistry::Get().Register(Name(PHYSICS_SYSTEM_NAME.data()), std::move(registration));
            if (!worldRegistered) {
                LOG_E("Jolt", "Physics subsystem registration failed");
                Shutdown();
            }
        }
        void Shutdown() override
        {
            if (started) {
                if (worldRegistered) {
                    WorldSubSystemRegistry::Get().Unregister(Name(PHYSICS_SYSTEM_NAME.data()));
                    worldRegistered = false;
                }
                PhysicsBackendRegistry::Get().Unregister();
                started = false;
            }
            backend.reset();
            lease.reset();
        }

    private:
        std::shared_ptr<const void>  lease;
        std::unique_ptr<JoltBackend> backend;
        bool                         started         = false;
        bool                         worldRegistered = false;
    };
} // namespace sky::phy
REGISTER_MODULE_TAGGED(sky::phy::JoltPhysicsModule)
