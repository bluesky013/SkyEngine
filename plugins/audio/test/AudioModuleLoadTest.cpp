//
// Created on 2026/09/21.
//

#include <audio/AudioRegistry.h>
#include <framework/application/ModuleManager.h>

#include <gtest/gtest.h>

using namespace sky;

TEST(AudioModuleLoadTest, LoadsModuleAndRegistersBackend)
{
    auto *registry = AudioRegistry::Get();
    registry->UnRegister();
    ASSERT_FALSE(registry->HasBackend());

    ModuleManager manager;
    manager.RegisterModule({"AudioModule", {}});
    manager.TopoSort();

    StartArguments args;
    manager.LoadModules(args);
    manager.StartModules();

    EXPECT_TRUE(registry->HasBackend());
    EXPECT_NE(registry->GetEngine(), nullptr);

    manager.UnLoadModules();
    EXPECT_FALSE(registry->HasBackend());
}
