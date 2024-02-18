//
// Created by blues on 2023/12/28.
//

#include <framework/application/ModuleManager.h>
#include <gtest/gtest.h>

using namespace sky;

TEST(ModuleManagerTest, ModuleSortTest)
{
    ModuleManager manager;

    manager.RegisterModule({"Test1", {"Test2", "Test3"}});
    manager.RegisterModule({"Test4", {"Test3"}});
    manager.RegisterModule({"Test5", {"Test1"}});
    manager.RegisterModule({"Test4", {"Test5"}});

    manager.TopoSort();
    manager.WalkModules([](const std::string &module) {
        printf("%s\n", module.c_str());
    });
}