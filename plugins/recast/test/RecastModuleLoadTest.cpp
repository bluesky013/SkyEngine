//
// Created on 2026/09/21.
//

#include <framework/application/ModuleManager.h>
#include <navigation/NaviMeshFactory.h>

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
    manager.LoadModules(args);
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
    manager.LoadModules(args);
    manager.StartModules();
    ASSERT_NE(factory->CreateNaviMesh(), nullptr);
    manager.UnLoadModules();
    ASSERT_EQ(factory->CreateNaviMesh(), nullptr);

    ModuleManager second;
    second.RegisterModule({"RecastNavigation", {}});
    second.TopoSort();
    second.LoadModules(args);
    second.StartModules();
    EXPECT_NE(factory->CreateNaviMesh(), nullptr);
    second.UnLoadModules();
}
