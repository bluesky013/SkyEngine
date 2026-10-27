//
// Created on 2026/10/27.
//

#include "PhysicsAssets.h"
#include <framework/application/ModuleManager.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/World.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>
#include <jolt/PhysicsSubSystemConfig.h>
#include <jolt/components/PhysicsBodyComponent.h>
#include <physics/IPhysicsSystem.h>
#include <physics/PhysicsBackendRegistry.h>
#include <physics/PhysicsMesh.h>

#include <iostream>
#include <string_view>

namespace {
    int RunModuleLifecycle()
    {
        using namespace sky;
        using namespace sky::phy;
        auto &registry = PhysicsBackendRegistry::Get();
        World::Reflect(SerializationContext::Get());
        const TypeNode                *metadata = nullptr;
        std::unique_ptr<IPhysicsWorld> retained;
        for (int cycle = 0; cycle < 2; ++cycle) {
            ModuleManager manager;
            manager.RegisterModule({"JoltPhysicsModule", {}});
            manager.RegisterModule({"JoltPhysicsRenderModule", {}});
            manager.TopoSort();
            if (!manager.LoadModules({})) {
                std::cerr << "Module loading failed\n";
                return 1;
            }
            manager.StartModules();
            auto *current = SerializationContext::Get()->FindType("PhysicsBodyData");
            if (!current || (metadata && current != metadata)) {
                std::cerr << "Metadata owner changed\n";
                return 1;
            }
            metadata = current;
            if (!registry.HasBackend() || !WorldSubSystemRegistry::Get().IsRegistered(Name(PHYSICS_SYSTEM_NAME.data()))) {
                return 1;
            }
            std::unique_ptr<World> host(World::CreateWorld());
            host->Init();
            PhysicsSubSystemConfig config;
            config.gravity       = -3;
            config.fixedTimestep = 0.025f;
            config.workerCount   = 0;
            Any  value(std::in_place_type<PhysicsSubSystemConfig>, config);
            auto subsystem = WorldSubSystemRegistry::Get().Create(Name(PHYSICS_SYSTEM_NAME.data()), *host, value);
            if (!subsystem) {
                std::cerr << "Configured subsystem creation failed\n";
                return 1;
            }
            auto           *physics = static_cast<IPhysicsSystem *>(subsystem.get());
            PhysicsBodyDesc body;
            body.kind                  = BodyKind::Dynamic;
            body.transform.translation = {0, 5, 0};
            auto id                    = physics->GetWorld()->CreateBody(body);
            physics->SingleStep();
            Vector3 linear, angular;
            if (!physics->GetWorld()->GetBodyVelocity(id, linear, angular) || std::abs(linear.y + 0.075f) > 0.005f) {
                std::cerr << "World config was not applied\n";
                return 1;
            }
            auto configNode      = SerializationContext::Get()->FindType("PhysicsSubSystemConfig");
            config.deterministic = true;
            Any unsupported(std::in_place_type<PhysicsSubSystemConfig>, config);
            if (!configNode || WorldSubSystemRegistry::Get().Create(Name(PHYSICS_SYSTEM_NAME.data()), *host, unsupported)) {
                return 1;
            }
            if (!retained) {
                retained.reset(registry.CreateWorld({}));
            }
            if (!retained) {
                return 1;
            }
            int providerCalls = 0;
            SetPhysicsMeshProvider([&](const Uuid &) -> CounterPtr<TriangleMesh> {
                ++providerCalls;
                return {};
            });
            CreatePhysicsMesh({});
            if (providerCalls != 1) {
                return 1;
            }
            manager.UnLoadModules();
            // Render shutdown must clear the shared provider even when it was replaced by the host.
            CreatePhysicsMesh({});
            if (providerCalls != 1 || registry.HasBackend() || WorldSubSystemRegistry::Get().IsRegistered(Name(PHYSICS_SYSTEM_NAME.data()))) {
                return 1;
            }
            if (!retained->Step(1.f / 60.f) || !physics->GetWorld()->Step(1.f / 60.f)) {
                std::cerr << "World did not survive module shutdown\n";
                return 1;
            }
        }
        retained.reset();
        std::cout << "Jolt module lifecycle: two sessions, one metadata owner, retained worlds and shared render provider passed\n";
        return 0;
    }
} // namespace

int main(int argc, char *argv[])
{
    if (argc == 2 && std::string_view(argv[1]) == "--module-lifecycle") {
        return RunModuleLifecycle();
    }
    sky::phy::RegisterPhysicsAssets();
    testing::InitGoogleTest(&argc, argv);
    sky::World::Reflect(sky::SerializationContext::Get());
    sky::phy::PhysicsBodyComponent::Reflect(sky::SerializationContext::Get());
    sky::phy::PhysicsSubSystemConfig::Reflect(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}
