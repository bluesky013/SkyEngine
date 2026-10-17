//
// ModuleManager: statically-registered (compiled-in) modules load without dlopen.
//

#include <framework/application/ModuleManager.h>
#include <framework/interface/IModule.h>

#include <gtest/gtest.h>

#include <memory>

namespace {

    class TestStaticModule : public sky::IModule {
    public:
        bool Init(const sky::StartArguments &) override
        {
            inited = true;
            return true;
        }
        void Start() override
        {
            started = true;
        }

        bool inited  = false;
        bool started = false;
    };

} // namespace

TEST(ModuleManagerStaticTest, StaticModuleLoadsWithoutDlopen)
{
    sky::ModuleManager manager;
    auto                module = std::make_unique<TestStaticModule>();
    TestStaticModule   *raw    = module.get();

    manager.AddStaticModule("TestStaticModule", std::move(module));
    manager.TopoSort();

    sky::StartArguments args;
    manager.LoadModules(args);
    manager.StartModules();

    EXPECT_TRUE(raw->inited);
    EXPECT_TRUE(raw->started);
}
