//
// Created on 2026/09/21.
//

#include <framework/application/ModuleManager.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <navigation/INavigationSystem.h>
#include <navigation/NaviMeshFactory.h>
#include <recast/NavigationSubSystemConfig.h>

#include <gtest/gtest.h>

using namespace sky;

TEST(RecastModuleLoadTest, LoadsAndRegistersFactory)
{
    auto *factory = ai::NaviMeshFactory::Get();
    factory->UnRegister();
    ASSERT_EQ(factory->CreateNaviMesh(), nullptr);

    ModuleManager manager;
    manager.RegisterModule({"RecastNavigation", {}});
    manager.TopoSort();

    StartArguments args;
    ASSERT_TRUE(manager.LoadModules(args));
    manager.StartModules();

    EXPECT_NE(factory->CreateNaviMesh(), nullptr);

    manager.UnLoadModules();
    EXPECT_EQ(factory->CreateNaviMesh(), nullptr);
}

TEST(RecastModuleLoadTest, ReloadAfterUnload)
{
    auto *factory = ai::NaviMeshFactory::Get();

    ModuleManager manager;
    manager.RegisterModule({"RecastNavigation", {}});
    manager.TopoSort();

    StartArguments args;
    ASSERT_TRUE(manager.LoadModules(args));
    manager.StartModules();
    ASSERT_NE(factory->CreateNaviMesh(), nullptr);
    manager.UnLoadModules();
    ASSERT_EQ(factory->CreateNaviMesh(), nullptr);

    ModuleManager second;
    second.RegisterModule({"RecastNavigation", {}});
    second.TopoSort();
    ASSERT_TRUE(second.LoadModules(args));
    second.StartModules();
    EXPECT_NE(factory->CreateNaviMesh(), nullptr);
    second.UnLoadModules();
}

TEST(RecastModuleLoadTest, RetainedObjectsRemainUsableAfterSessionShutdown)
{
    ModuleManager manager;
    manager.RegisterModule({"RecastNavigation", {}});
    manager.TopoSort();
    ASSERT_TRUE(manager.LoadModules({}));
    manager.StartModules();
    const auto mesh      = ai::NaviMeshFactory::Get()->CreateNaviMesh();
    const auto filter    = ai::NaviMeshFactory::Get()->CreateQueryFilter();
    const auto generator = ai::NaviMeshFactory::Get()->CreateGenerator();
    ASSERT_NE(mesh, nullptr);
    ASSERT_NE(filter, nullptr);
    ASSERT_NE(generator, nullptr);

    manager.UnLoadModules();
    EXPECT_EQ(ai::NaviMeshFactory::Get()->CreateNaviMesh(), nullptr);
    filter->SetIncludeFlags(3);
    EXPECT_EQ(filter->GetIncludeFlags(), 3);
    ai::NaviMeshBuildParams params;
    params.bounds = {{0, -1, 0}, {10, 2, 10}};
    EXPECT_TRUE(mesh->PrepareStreaming(params));
    ai::NaviDebugGeometry geometry;
    mesh->BuildDebugGeometry(geometry);
    EXPECT_TRUE(geometry.vertices.empty());
    generator->Setup({});
    generator->StartAsync();
    TaskExecutor::Get()->WaitForAll();
    EXPECT_FALSE(generator->HasSucceeded());
    EXPECT_FALSE(generator->GetError().empty());
}

TEST(RecastModuleLoadTest, FailedSubsystemRegistrationDoesNotRemoveAnotherOwner)
{
    auto *factory = ai::NaviMeshFactory::Get();
    factory->UnRegister();
    const Name                 name(ai::INavigationSystem::NAME.data());
    auto                      &registry = WorldSubSystemRegistry::Get();
    WorldSubSystemRegistration registration;
    registration.factory = [](World &, const Any &) { return std::make_unique<IWorldSubSystem>(); };
    ASSERT_TRUE(registry.Register(name, std::move(registration)));
    const auto   *owner = registry.GetRegistration(name);
    ModuleManager manager;
    manager.RegisterModule({"RecastNavigation", {}});
    manager.TopoSort();
    ASSERT_TRUE(manager.LoadModules({}));
    manager.StartModules();
    EXPECT_EQ(factory->CreateNaviMesh(), nullptr);
    manager.UnLoadModules();
    EXPECT_EQ(registry.GetRegistration(name), owner);
    registry.Unregister(name);
}

TEST(RecastModuleLoadTest, InvalidWorldConfigIsRejectedByValidatorAndFactory)
{
    ModuleManager manager;
    manager.RegisterModule({"RecastNavigation", {}});
    manager.TopoSort();
    ASSERT_TRUE(manager.LoadModules({}));
    manager.StartModules();
    const Name  name(ai::INavigationSystem::NAME.data());
    auto       &registry     = WorldSubSystemRegistry::Get();
    const auto *registration = registry.GetRegistration(name);
    ASSERT_NE(registration, nullptr);
    ai::NavigationSubSystemConfig config;
    config.maxTiles = 0;
    Any         value(std::in_place_type<ai::NavigationSubSystemConfig>, config);
    std::string error;
    EXPECT_FALSE(registration->validate(value, error));
    EXPECT_FALSE(error.empty());
    WorldPtr world = World::CreateWorld();
    EXPECT_EQ(registry.Create(name, *world, value), nullptr);
    manager.UnLoadModules();
}
