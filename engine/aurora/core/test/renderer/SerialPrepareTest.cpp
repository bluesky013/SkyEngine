#include "prepare/SerialPrepareExecutor.h"
#include <core/ecs/View.h>
#include <gtest/gtest.h>

#include <array>
#include <thread>

namespace prepare_test {
    struct Position {
        int x = 0;
    };
    struct Bounds {
        int min = 0;
        int max = 0;
    };
} // namespace prepare_test
SKY_TYPE_TAG(prepare_test::Position, "sky.test.prepare.Position")
SKY_TYPE_TAG(prepare_test::Bounds, "sky.test.prepare.Bounds")

namespace {
    using namespace sky;
    using namespace sky::aurora;
    using namespace prepare_test;

    DependencyPlan Compile(std::vector<SystemDesc> systems)
    {
        auto result = CompileDependencyPlan(systems);
        if (auto *error = std::get_if<DependencyDiagnostic>(&result)) {
            ADD_FAILURE() << error->message;
            return {};
        }
        return std::get<DependencyPlan>(std::move(result));
    }
} // namespace

TEST(SerialPrepare, TwoScenesAndTwoViewsUseOneScenePreparationEach)
{
    EntityRegistry sceneA;
    EntityRegistry sceneB;
    const auto     add = [](EntityRegistry &scene, int x) {
        const auto id = scene.CreateEntity();
        scene.Add<Position>(id, {x});
        scene.Add<Bounds>(id, {});
        return id;
    };
    const auto a0 = add(sceneA, 1);
    const auto a1 = add(sceneA, 10);
    add(sceneB, 20);
    SystemDesc boundsA{1, "BoundsA"};
    boundsA.accesses = {ComponentAccess<Position>("scene", {1, 1}, AccessMode::READ), ComponentAccess<Bounds>("scene", {1, 1}, AccessMode::WRITE)};
    SystemDesc boundsB{2, "BoundsB"};
    boundsB.accesses = {ComponentAccess<Position>("scene", {2, 1}, AccessMode::READ), ComponentAccess<Bounds>("scene", {2, 1}, AccessMode::WRITE)};
    SystemDesc viewA{3, "ViewA"};
    viewA.accesses = {ComponentAccess<Bounds>("scene", {1, 1}, AccessMode::READ), ResourceAccess("view", {1, 1}, "visibility", AccessMode::WRITE)};
    viewA.after    = {1};
    auto viewB     = viewA;
    viewB.id       = 4;
    viewB.name     = "ViewB";
    viewB.accesses[1].key.owner.instance = 2;
    const auto plan                      = Compile({viewB, boundsB, viewA, boundsA});
    ASSERT_EQ(plan.systems.size(), 4u);
    for (const auto &edge : plan.edges) {
        EXPECT_EQ(plan.systems[edge.from].id, 1u);
        EXPECT_TRUE(plan.systems[edge.to].id == 3 || plan.systems[edge.to].id == 4);
    }
    std::array<int, 4>                   runs{};
    std::array<std::vector<EntityId>, 2> visible;
    const auto                           caller = std::this_thread::get_id();
    const auto                           kernel = [&](SystemId id) {
        EXPECT_EQ(std::this_thread::get_id(), caller);
        ++runs[id - 1];
        if (id <= 2) {
            auto &scene = id == 1 ? sceneA : sceneB;
            scene.View<Position, Bounds>().ForEach(
                [](EntityId, const Position &position, Bounds &bounds) { bounds = {position.x - 1, position.x + 1}; });
        } else {
            const auto view    = id - 3;
            const int  minimum = view == 0 ? 0 : 5;
            visible[view].clear();
            sceneA.View<Bounds>().ForEach([&](EntityId entity, const Bounds &bounds) {
                if (bounds.max >= minimum) {
                    visible[view].push_back(entity);
                }
            });
        }
    };
    EXPECT_TRUE(ExecuteSerialPrepare(plan, kernel).Succeeded());
    EXPECT_EQ(runs, (std::array<int, 4>{1, 1, 1, 1}));
    EXPECT_EQ(visible[0], (std::vector<EntityId>{a0, a1}));
    EXPECT_EQ(visible[1], (std::vector<EntityId>{a1}));
    sceneA.DestroyEntity(a1);
    EXPECT_TRUE(ExecuteSerialPrepare(plan, kernel).Succeeded());
    EXPECT_EQ(runs, (std::array<int, 4>{2, 2, 2, 2}));
    EXPECT_EQ(visible[0], (std::vector<EntityId>{a0}));
    EXPECT_TRUE(visible[1].empty());
}

TEST(SerialPrepare, FailureSkipsTransitiveConsumersAndFinishesIndependentNodes)
{
    SystemDesc source{1, "FailingBounds"};
    SystemDesc view{2, "View"};
    view.after = {1};
    SystemDesc compose{3, "Compose"};
    compose.after = {2};
    SystemDesc            independent{4, "OtherScene"};
    const auto            plan = Compile({source, view, compose, independent});
    std::vector<SystemId> executed;
    auto                  result = ExecuteSerialPrepareChecked(plan, [&](SystemId id) {
        executed.push_back(id);
        if (id == 1) {
            return PrepareNodeResult{id, PrepareNodeState::FAILED, "bounds failed"};
        }
        return PrepareNodeResult{id, PrepareNodeState::SUCCEEDED, {}};
    });
    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(executed, (std::vector<SystemId>{1, 4}));
    ASSERT_EQ(result.nodes.size(), 4u);
    EXPECT_EQ(result.nodes[0].state, PrepareNodeState::FAILED);
    EXPECT_EQ(result.nodes[0].diagnostic, "bounds failed");
    EXPECT_EQ(result.nodes[1].state, PrepareNodeState::SKIPPED);
    EXPECT_EQ(result.nodes[2].state, PrepareNodeState::SKIPPED);
    EXPECT_EQ(result.nodes[3].state, PrepareNodeState::SUCCEEDED);
    result = ExecuteSerialPrepareChecked(Compile({source}),
                                         [](SystemId id) { return PrepareNodeResult{id, PrepareNodeState::FAILED, "explicit failure"}; });
    ASSERT_EQ(result.nodes.size(), 1u);
    EXPECT_EQ(result.nodes[0].state, PrepareNodeState::FAILED);
    EXPECT_FALSE(result.nodes[0].diagnostic.empty());
}

TEST(SerialPrepare, EmptyAndInvalidPlansDoNotInvokeKernels)
{
    EXPECT_TRUE(ExecuteSerialPrepare(Compile({}), {}).Succeeded());
    auto plan = Compile({SystemDesc{1, "A"}, SystemDesc{2, "B"}});
    EXPECT_FALSE(ExecuteSerialPrepare(plan, {}).Succeeded());
    int calls = 0;
    plan.edges.push_back({1, 0, DependencyOrigin::AFTER});
    EXPECT_FALSE(ExecuteSerialPrepare(plan, [&](SystemId) { ++calls; }).Succeeded());
    EXPECT_EQ(calls, 0);
    plan.edges[0] = {0, 99, DependencyOrigin::AFTER};
    EXPECT_FALSE(ExecuteSerialPrepare(plan, [&](SystemId) { ++calls; }).Succeeded());
    EXPECT_EQ(calls, 0);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
