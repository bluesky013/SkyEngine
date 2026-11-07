#include <framework/application/ModuleManager.h>
#include <framework/world/World.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>

using namespace sky;

TEST(ParticleModules, WorldSubsystemRegistersAndUnregisters)
{
    auto manager = std::make_unique<ModuleManager>();
    manager->RegisterModule({"Content.Module", {"Content.Metadata"}});
    manager->RegisterModule({"Particle.Assets", {}});
    manager->RegisterModule({"Particle.World", {"Particle.Assets"}});
    manager->TopoSort();
    StartArguments arguments;
    arguments.args = {"Particle.ModuleTest"};
    ASSERT_TRUE(manager->LoadModules(arguments));
    manager->StartModules();
    ASSERT_TRUE(WorldSubSystemRegistry::Get().IsRegistered(Name("Particle")));
    auto world = WorldPtr(World::CreateWorld());
    WorldDesc desc;
    desc.subSystems.push_back({Name("Particle"), {}, true});
    ASSERT_TRUE(world->Build(desc));
    EXPECT_NE(world->GetSubSystem(Name("Particle")), nullptr);
    world.Reset(nullptr);
    ASSERT_TRUE(manager->UnLoadModules());
    EXPECT_FALSE(WorldSubSystemRegistry::Get().IsRegistered(Name("Particle")));
}

