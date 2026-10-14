#include <aurora/renderer/IRenderer.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/Interface.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

#if defined(AURORA_RENDERER_STATIC_FIXTURE)
extern "C" IModule *sky_module_create_Aurora_Renderer_Contract();
#elif defined(AURORA_RENDERER_PACKAGED_STATIC)
extern "C" IModule *sky_module_create_Aurora_Renderer();
#endif

namespace {
    using ProviderService = Interface<IRendererProvider>;

    struct LoadedProvider {
        ModuleManager manager;
        LoadedProvider()
        {
#if defined(AURORA_RENDERER_STATIC_FIXTURE)
            manager.AddStaticModule("Aurora.Renderer", std::unique_ptr<IModule>(sky_module_create_Aurora_Renderer_Contract()));
#elif defined(AURORA_RENDERER_PACKAGED_STATIC)
            manager.AddStaticModule("Aurora.Renderer", std::unique_ptr<IModule>(sky_module_create_Aurora_Renderer()));
#else
            manager.RegisterModule({"Aurora.Renderer", {}});
#endif
            manager.TopoSort();
            manager.LoadModules({});
            manager.StartModules();
        }
        ~LoadedProvider()
        {
            manager.UnLoadModules();
        }
    };
} // namespace

TEST(RendererModule, RegistersInHostEnvironmentAndReloads)
{
    // Obtain the service in the host BEFORE dlopen: catches per-DLL singletons.
    auto *service = ProviderService::Get();
    ASSERT_EQ(service->GetApi(), nullptr);
    for (int iteration = 0; iteration < 2; ++iteration) {
        {
            LoadedProvider loaded;
            auto          *provider = service->GetApi();
            ASSERT_NE(provider, nullptr);
            auto result = provider->Create({});
            ASSERT_TRUE(std::holds_alternative<RendererError>(result));
            EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::NOT_READY);
            EXPECT_FALSE(std::get<RendererError>(result).message.empty());
        }
        EXPECT_EQ(service->GetApi(), nullptr);
    }
}

TEST(RendererModule, RejectsZeroCountsAndBudgets)
{
    LoadedProvider loaded;
    auto          *provider = ProviderService::Get()->GetApi();
    ASSERT_NE(provider, nullptr);
    for (auto member : {&RendererInitInfo::inflightFrames, &RendererInitInfo::maxPendingPackets, &RendererInitInfo::prepareWorkers,
                        &RendererInitInfo::encodeWorkers}) {
        RendererInitInfo info;
        info.*member = 0;
        auto result  = provider->Create(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(result));
        EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::INVALID_ARGUMENT);
    }
    for (auto member : {&RendererInitInfo::maxPendingResourceBytes, &RendererInitInfo::maxUploadBytesPerFrame}) {
        RendererInitInfo info;
        info.*member = 0;
        auto result  = provider->Create(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(result));
        EXPECT_EQ(std::get<RendererError>(result).code, RendererErrorCode::INVALID_ARGUMENT);
    }
}

#if defined(AURORA_RENDERER_STATIC_FIXTURE)
TEST(RendererModule, DuplicateRegistrationDoesNotReplaceOrUnregisterOwner)
{
    auto owner     = std::unique_ptr<IModule>(sky_module_create_Aurora_Renderer_Contract());
    auto duplicate = std::unique_ptr<IModule>(sky_module_create_Aurora_Renderer_Contract());
    ASSERT_TRUE(owner->Init({}));
    auto *provider = ProviderService::Get()->GetApi();
    EXPECT_TRUE(owner->Init({}));
    EXPECT_FALSE(duplicate->Init({}));
    duplicate->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), provider);
    owner->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), nullptr);
    owner->Shutdown();
    EXPECT_EQ(ProviderService::Get()->GetApi(), nullptr);
}
#endif

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
