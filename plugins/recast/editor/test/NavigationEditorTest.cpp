//
// Created on 2026/10/30.
//

#include <recast/editor/NavigationEditorService.h>

#include <atomic>
#include <editor/core/document/EditorWorldContext.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <navigation/NaviMeshFactory.h>
#include <navigation/NavigationSystem.h>

using namespace sky;
using namespace sky::ai;
using namespace sky::editor;

namespace {
    class AsyncPreparationComponent final : public ComponentBase {
    public:
        COMPONENT_RUNTIME_INFO(AsyncPreparationComponent)

        ComponentPreparation PollWorldPreparation(std::string &error) override
        {
            ++polls;
            if (fail) {
                error = "Collision source failed to load";
                return ComponentPreparation::Failed;
            }
            error.clear();
            if (pendingPolls) {
                --pendingPolls;
                return ComponentPreparation::Preparing;
            }
            return ComponentPreparation::Ready;
        }
        void Tick(float) override
        {
            ++ticks;
        }

        uint32_t pendingPolls = 0, polls = 0, ticks = 0;
        bool     fail = false;
    };

    class NavigationEditorMetadata final : public testing::Environment {
    public:
        void SetUp() override
        {
            SerializationContext::Get()->Register<AsyncPreparationComponent>("NavigationEditorAsyncPreparationComponent");
        }
    };
    const auto *metadata = testing::AddGlobalTestEnvironment(new NavigationEditorMetadata());

    class TestMesh final : public NaviMesh {
    public:
        NaviQueryResult FindPath(const Vector3 &, const Vector3 &, const NaviQueryFilterPtr &, const NaviPathQueryParam &) const override
        {
            return NaviQueryResult::FAILED;
        }
        NaviQueryResult QueryPath(const Vector3 &, const Vector3 &, const NaviQueryFilterPtr &, const NaviPathQueryParam &, NaviPath &) const override
        {
            return NaviQueryResult::FAILED;
        }
        void BuildDebugGeometry(NaviDebugGeometry &) const override
        {
        }
        bool LoadData(const NaviMeshData &) override
        {
            return true;
        }
        bool ApplyBuild(const NaviMeshData &, const std::vector<NaviMeshTileCoord> &) override
        {
            return true;
        }
        bool SaveFullData(std::vector<uint8_t> &) const override
        {
            return true;
        }
        bool AddTile(const NaviMeshTilePayload &) override
        {
            return true;
        }
        bool RemoveTile(const NaviMeshTileCoord &) override
        {
            return true;
        }
        bool PrepareStreaming(const NaviMeshBuildParams &) override
        {
            return true;
        }
    };
    struct BuildCounts {
        uint32_t             created = 0, setup = 0;
        std::atomic_uint32_t completed{0};
    };
    class TestGenerator final : public NaviMeshGenerator {
    public:
        explicit TestGenerator(BuildCounts &value) : counts(value)
        {
        }
        void Setup(const WorldPtr &) override
        {
            ++counts.setup;
        }
        bool HasSucceeded() const override
        {
            return succeeded.load();
        }
        std::string_view GetError() const override
        {
            return {};
        }
        NaviMeshBuildParams GetBuildParams() const override
        {
            return {};
        }
        void CollectTiles(NaviMeshData &) const override
        {
        }

    private:
        bool DoWork() override
        {
            ++counts.completed;
            succeeded.store(true);
            return true;
        }
        BuildCounts     &counts;
        std::atomic_bool succeeded{false};
    };
    class TestFactory final : public NaviMeshFactory::Impl {
    public:
        explicit TestFactory(BuildCounts &value) : counts(value)
        {
        }
        NaviMesh *CreateNaviMesh() override
        {
            return new TestMesh();
        }
        NaviMeshGenerator *CreateGenerator() override
        {
            ++counts.created;
            return new TestGenerator(counts);
        }
        NaviQueryFilter *CreateQueryFilter() override
        {
            return nullptr;
        }

    private:
        BuildCounts &counts;
    };
    class NavigationEditorTest : public testing::Test {
    protected:
        void SetUp() override
        {
            EditorActionRegistry::Get()->Clear();
            ASSERT_TRUE(NaviMeshFactory::Get()->Register(new TestFactory(counts)));
            world = World::CreateWorld();
            world->AddSubSystem(Name(INavigationSystem::NAME.data()), new NavigationSystem());
            EditorWorldContext::Get()->SetWorld(world.Get(), true);
            service = std::make_shared<NavigationEditorService>();
            ASSERT_TRUE(service->Register(std::make_shared<int>(1)));
        }
        void TearDown() override
        {
            service->Unregister();
            TaskExecutor::Get()->WaitForAll();
            SceneOverlayRegistry::Get()->OnRendererStopped();
            EditorWorldContext::Get()->Clear();
            service.reset();
            world = nullptr;
            NaviMeshFactory::Get()->UnRegister();
            EditorActionRegistry::Get()->Clear();
        }
        BuildCounts                              counts;
        WorldPtr                                 world;
        std::shared_ptr<NavigationEditorService> service;
    };
} // namespace

TEST_F(NavigationEditorTest, DefersBuildUntilTickAndPublishesCompletion)
{
    ASSERT_TRUE(service->CanBuild());
    EXPECT_TRUE(EditorActionRegistry::Get()->Invoke("navigation.build"));
    EXPECT_EQ(counts.created, 0u);
    EXPECT_EQ(counts.setup, 0u);
    EXPECT_FALSE(service->CanBuild());
    service->Tick();
    EXPECT_EQ(counts.created, 1u);
    EXPECT_EQ(counts.setup, 1u);
    TaskExecutor::Get()->WaitForAll();
    service->Tick();
    EXPECT_EQ(counts.completed.load(), 1u);
    EXPECT_TRUE(service->IsDebugVisible());
    EXPECT_TRUE(service->CanBuild());
}

TEST_F(NavigationEditorTest, DropsRequestWhenWorldClosesBeforeTick)
{
    service->RequestBuild();
    EditorWorldContext::Get()->Clear();
    service->Tick();
    EXPECT_EQ(counts.created, 0u);
    EXPECT_FALSE(service->CanBuild());
}

TEST_F(NavigationEditorTest, PlayingDisablesBuildAndUnregisterRemovesActions)
{
    EditorWorldContext::Get()->SetWorld(world.Get(), false);
    EXPECT_FALSE(service->CanBuild());
    EditorActionRegistry::Get()->Invoke("navigation.build");
    service->Tick();
    EXPECT_EQ(counts.created, 0u);
    EXPECT_TRUE(EditorActionRegistry::Get()->Find("navigation.debug"));
    service->Unregister();
    EXPECT_FALSE(EditorActionRegistry::Get()->Find("navigation.build"));
    EXPECT_FALSE(EditorActionRegistry::Get()->Find("navigation.debug"));
    SceneOverlayRegistry::Get()->OnRendererStopped();
    EXPECT_TRUE(service->Register(std::make_shared<int>(2)));
}

TEST_F(NavigationEditorTest, DeactivatedSessionCannotRegisterOrRunStagedRequests)
{
    service->RequestBuild();
    service->Deactivate();
    EXPECT_FALSE(service->IsActive());
    EXPECT_FALSE(service->Register(std::make_shared<int>(2)));
    EXPECT_FALSE(EditorActionRegistry::Get()->Find("navigation.build"));
    EXPECT_FALSE(EditorActionRegistry::Get()->Find("navigation.debug"));
    EXPECT_FALSE(service->CanBuild());
    service->Tick();
    EXPECT_EQ(counts.created, 0u);
    SceneOverlayRegistry::Get()->OnRendererStopped();
    auto nextSession = std::make_shared<NavigationEditorService>();
    EXPECT_TRUE(nextSession->Register(std::make_shared<int>(3)));
    nextSession->Unregister();
    SceneOverlayRegistry::Get()->OnRendererStopped();
}

TEST_F(NavigationEditorTest, PollsDeferredComponentsAcrossFramesBeforeStartingBuild)
{
    auto               *actor       = world->CreateActor("LoadingGeometry", false);
    Actor::ComponentPtr component   = std::make_unique<AsyncPreparationComponent>();
    auto               *preparation = static_cast<AsyncPreparationComponent *>(component.get());
    preparation->pendingPolls       = 2;
    ASSERT_TRUE(actor->AttachComponent(component));
    service->RequestBuild();
    service->Tick();
    EXPECT_EQ(preparation->polls, 1u);
    EXPECT_EQ(counts.created, 0u);
    EXPECT_FALSE(service->CanBuild());
    service->Tick();
    EXPECT_EQ(preparation->polls, 2u);
    EXPECT_EQ(counts.created, 0u);
    service->Tick();
    EXPECT_EQ(preparation->polls, 3u);
    EXPECT_EQ(preparation->ticks, 0u);
    EXPECT_EQ(counts.created, 1u);
    TaskExecutor::Get()->WaitForAll();
    service->Tick();
    EXPECT_EQ(counts.completed.load(), 1u);
    EXPECT_TRUE(service->IsDebugVisible());
}

TEST_F(NavigationEditorTest, PreparationFailureReportsActorAndComponentAndAllowsRetry)
{
    auto               *actor       = world->CreateActor("BrokenGeometry", false);
    Actor::ComponentPtr component   = std::make_unique<AsyncPreparationComponent>();
    auto               *preparation = static_cast<AsyncPreparationComponent *>(component.get());
    preparation->fail               = true;
    const auto type                 = preparation->GetTypeId().ToString();
    ASSERT_TRUE(actor->AttachComponent(component));
    service->RequestBuild();
    service->Tick();
    EXPECT_EQ(counts.created, 0u);
    EXPECT_NE(service->GetError().find("BrokenGeometry"), std::string::npos);
    EXPECT_NE(service->GetError().find(type), std::string::npos);
    EXPECT_NE(service->GetError().find("Collision source failed to load"), std::string::npos);
    EXPECT_TRUE(service->CanBuild());
    preparation->fail = false;
    service->RequestBuild();
    EXPECT_TRUE(service->GetError().empty());
    service->Tick();
    EXPECT_EQ(counts.created, 1u);
    EXPECT_EQ(preparation->ticks, 0u);
}
