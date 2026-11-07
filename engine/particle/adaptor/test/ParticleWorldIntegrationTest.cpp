#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProduct.h>
#include <framework/interface/Interface.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>
#include <gtest/gtest.h>
#include <particle/ParticleRenderTypes.h>
#include <particle/ParticleView.h>
#include <particle/assets/ParticleEffectAsset.h>
#include <particle/builder/ParticleEffectBuilder.h>
#include <particle/world/ParticleComponent.h>
#include <particle/world/ParticleSystem.h>

#include <filesystem>

using namespace sky;
using namespace sky::particle;

namespace {
    class TestView final : public IParticleViewProvider {
    public:
        ParticleViewInfo view;
        mutable Uuid     requestedWorld;
        bool             GetView(const Uuid &world, ParticleViewInfo &out) const override
        {
            requestedWorld = world;
            out            = view;
            return view.valid;
        }
    };

    class TestSink final : public IParticleRenderSink {
    public:
        std::shared_ptr<const ParticleRenderFrame> frame;
        std::vector<Uuid>                          removed;
        void                                       Submit(std::shared_ptr<const ParticleRenderFrame> value) override
        {
            frame = std::move(value);
        }
        void RemoveWorld(const Uuid &id) override
        {
            removed.push_back(id);
            if (frame && frame->world == id)
                frame.reset();
        }
    };

    std::string EffectSource(std::string_view space, uint32_t size)
    {
        return "{\"version\":1,\"emitters\":[{\"name\":\"sparks\",\"simTarget\":\"CPU\",\"space\":\"" + std::string(space) +
               "\",\"capacity\":32,\"spawn\":{\"rate\":20},\"modules\":[{\"type\":\"INITIALIZE\",\"params\":{\"lifetime\":100,\"size\":" +
               std::to_string(size) + "}}],\"renderers\":[{\"type\":\"SPRITE\",\"blend\":\"ALPHA\"}]}]}";
    }

    class ParticleWorldIntegration : public testing::Test {
    protected:
        // Only this suite cooks effects, so the builder registration stays out of the shared main.
        static void SetUpTestSuite()
        {
            RegisterParticleBuilder();
        }

        void SetUp() override
        {
            std::error_code error;
            root = std::filesystem::temp_directory_path(error) / ("sky-particle-world-" + Uuid::Create().ToString());
            ASSERT_FALSE(error);
            sourceFs = new NativeFileSystem(FilePath(root / "assets"));
            products = new NativeFileSystem(FilePath(root / "products"));
            AssetDataBase::Get()->SetWorkSpaceFs(sourceFs);
            AssetManager::Get()->SetSourceCatalog(AssetDataBase::Get());
            AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(products, "particle-world"));
            view.view = {{0.f, 0.f, 0.f}, true};
            Interface<IParticleViewProvider>::Get()->Register(view);
            Interface<IParticleRenderSink>::Get()->Register(sink);
            world  = WorldPtr(World::CreateWorld());
            system = new ParticleSystem();
            world->AddSubSystem(Name(ParticleSystem::NAME.data()), system);
            auto *actor = world->CreateActor();
            ASSERT_TRUE(actor);
            transform = actor->GetComponent<TransformComponent>();
            component = actor->AddComponent<ParticleComponent>();
            ASSERT_TRUE(transform);
            ASSERT_TRUE(component);
            world->StartSimulation();
        }

        void TearDown() override
        {
            world.Reset(nullptr);
            AssetExecutor::Get()->WaitForAll();
            Interface<IParticleRenderSink>::Get()->UnRegister();
            Interface<IParticleViewProvider>::Get()->UnRegister();
            AssetDataBase::Get()->Reset();
            AssetManager::Get()->SetSourceCatalog(nullptr);
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }

        Uuid Cook(const char *path, const std::string &text)
        {
            const auto file = sourceFs->CreateOrOpenFile(path);
            if (!file)
                return {};
            {
                const auto stream = file->WriteAsArchive();
                if (!stream || !stream->SaveRaw(text.data(), text.size()))
                    return {};
                stream->Flush();
            }
            const auto info = AssetDataBase::Get()->RegisterAsset(path, false);
            if (!info)
                return {};
            AssetBuilderManager::Get()->BuildRequestSync(info->uuid, "particle-world");
            return info->uuid;
        }

        void Advance(float delta)
        {
            world->Tick(0.f);
            AssetExecutor::Get()->WaitForAll();
            world->Tick(delta);
        }

        std::filesystem::path root;
        NativeFileSystemPtr   sourceFs;
        NativeFileSystemPtr   products;
        TestView              view;
        TestSink              sink;
        WorldPtr              world;
        ParticleSystem       *system    = nullptr;
        TransformComponent   *transform = nullptr;
        ParticleComponent    *component = nullptr;
    };
} // namespace

TEST_F(ParticleWorldIntegration, LoadsEffectAppliesCullBudgetAndPublishesSnapshot)
{
    const auto effect = Cook("spark.particle", EffectSource("WORLD", 1));
    ASSERT_TRUE(effect);
    component->SetEffect(effect);
    Advance(.5f);
    EXPECT_GT(system->GetActiveParticleCount(), 0u);
    ASSERT_TRUE(sink.frame);
    EXPECT_EQ(sink.frame->world, world->GetRuntimeId());
    EXPECT_EQ(view.requestedWorld, world->GetRuntimeId());
    EXPECT_FALSE(sink.frame->emitters.empty());
    const auto retained = sink.frame;
    component->SetBudget(1);
    Advance(.5f);
    EXPECT_LE(system->GetActiveParticleCount(), 1u);
    EXPECT_GT(retained->emitters.front().instances.size(), 1u);
    transform->SetLocalTranslation({500.f, 0.f, 0.f});
    Advance(.5f);
    ASSERT_TRUE(sink.frame);
    EXPECT_TRUE(sink.frame->emitters.empty());
}

TEST_F(ParticleWorldIntegration, EffectReplacementAndProductReloadReplacePrograms)
{
    const auto first  = Cook("first.particle", EffectSource("WORLD", 1));
    const auto second = Cook("second.particle", EffectSource("WORLD", 2));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    component->SetEffect(first);
    Advance(.1f);
    ASSERT_TRUE(sink.frame);
    ASSERT_FALSE(sink.frame->emitters.empty());
    const auto originalHash = sink.frame->emitters.front().programHash;
    component->SetEffect(second);
    Advance(.1f);
    ASSERT_FALSE(sink.frame->emitters.empty());
    EXPECT_NE(sink.frame->emitters.front().programHash, originalHash);
    ASSERT_FALSE(sink.frame->emitters.front().instances.empty());
    EXPECT_FLOAT_EQ(sink.frame->emitters.front().instances.front().size, 2.f);
    EXPECT_EQ(Cook("second.particle", EffectSource("WORLD", 3)), second);
    Advance(.1f);
    ASSERT_FALSE(sink.frame->emitters.front().instances.empty());
    EXPECT_FLOAT_EQ(sink.frame->emitters.front().instances.front().size, 3.f);
    EXPECT_TRUE(system->GetLastError().empty());
}

TEST_F(ParticleWorldIntegration, WorldParticlesStayAtSpawnTransformAndLocalParticlesFollowActor)
{
    const auto worldEffect = Cook("world.particle", EffectSource("WORLD", 1));
    const auto localEffect = Cook("local.particle", EffectSource("LOCAL", 1));
    ASSERT_TRUE(worldEffect);
    ASSERT_TRUE(localEffect);
    transform->SetLocalTranslation({10.f, 0.f, 0.f});
    component->SetEffect(worldEffect);
    Advance(.1f);
    ASSERT_TRUE(sink.frame);
    ASSERT_FALSE(sink.frame->emitters.front().instances.empty());
    EXPECT_FLOAT_EQ(sink.frame->emitters.front().instances.front().position.x, 10.f);
    const auto count = system->GetActiveParticleCount();
    component->SetRateScale(0.f);
    transform->SetLocalTranslation({20.f, 0.f, 0.f});
    Advance(.1f);
    EXPECT_EQ(system->GetActiveParticleCount(), count);
    EXPECT_FLOAT_EQ(sink.frame->emitters.front().instances.front().position.x, 10.f);
    component->SetEffect(localEffect);
    component->SetRateScale(1.f);
    Advance(.1f);
    ASSERT_FALSE(sink.frame->emitters.front().instances.empty());
    EXPECT_FLOAT_EQ(sink.frame->emitters.front().instances.front().position.x, 20.f);
    component->SetRateScale(0.f);
    transform->SetLocalTranslation({30.f, 0.f, 0.f});
    Advance(.1f);
    EXPECT_FLOAT_EQ(sink.frame->emitters.front().instances.front().position.x, 30.f);
}

TEST_F(ParticleWorldIntegration, StopAndDetachWithdrawOnlyTheirWorldSnapshot)
{
    component->SetEffect(Cook("spark.particle", EffectSource("WORLD", 1)));
    Advance(.1f);
    ASSERT_TRUE(sink.frame);
    const auto id = world->GetRuntimeId();
    world->StopSimulation();
    EXPECT_FALSE(sink.frame);
    ASSERT_FALSE(sink.removed.empty());
    EXPECT_EQ(sink.removed.back(), id);
    world->StartSimulation();
    Advance(.1f);
    ASSERT_TRUE(sink.frame);
    world.Reset(nullptr);
    EXPECT_FALSE(sink.frame);
    EXPECT_EQ(sink.removed.back(), id);
}

TEST_F(ParticleWorldIntegration, UnrelatedProductRefreshPreservesLiveEmitterState)
{
    const auto effect = Cook("spark.particle", EffectSource("WORLD", 1));
    ASSERT_TRUE(effect);
    component->SetEffect(effect);
    Advance(.1f);
    const auto count = system->GetActiveParticleCount();
    ASSERT_GT(count, 0u);
    component->SetRateScale(0.f);
    ASSERT_TRUE(Cook("unrelated.particle", EffectSource("WORLD", 2)));
    Advance(.1f);
    EXPECT_EQ(system->GetActiveParticleCount(), count);
    EXPECT_TRUE(system->GetLastError().empty());
}
