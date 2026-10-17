//
// Created on 2026/10/07.
//

#include <framework/world/World.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace sky;

namespace {

    struct TestSubSystem : IWorldSubSystem {
        int marker = 0;
    };

    struct SimSubSystem : IWorldSubSystem {
        int  starts = 0;
        int  stops  = 0;
        void StartSimulation() override
        {
            ++starts;
        }
        void StopSimulation() override
        {
            ++stops;
        }
    };

    std::vector<Name> *gAttachOrder = nullptr;

    struct OrderedSubSystem : IWorldSubSystem {
        Name tag;
        explicit OrderedSubSystem(Name inTag) : tag(inTag)
        {
        }
        void OnAttachToWorld(World &) override
        {
            if (gAttachOrder != nullptr) {
                gAttachOrder->push_back(tag);
            }
        }
    };

    class WorldSubSystemRegistryTest : public ::testing::Test {
    protected:
        void SetUp() override
        {
            WorldSubSystemRegistry::Get().Clear();
            gAttachOrder = nullptr;
        }
        void TearDown() override
        {
            WorldSubSystemRegistry::Get().Clear();
            gAttachOrder = nullptr;
        }
    };

    WorldSubSystemRegistration MakeRegistration(int marker)
    {
        return WorldSubSystemRegistration{
            [marker](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> {
                auto sub    = std::make_unique<TestSubSystem>();
                sub->marker = marker;
                return sub;
            },
            nullptr,
            {},
            {},
        };
    }

} // namespace

TEST_F(WorldSubSystemRegistryTest, RegisterCreateAndOverride)
{
    auto &registry = WorldSubSystemRegistry::Get();
    EXPECT_TRUE(registry.Register(Name("Test"), MakeRegistration(1)));
    EXPECT_TRUE(registry.IsRegistered(Name("Test")));
    EXPECT_NE(registry.GetRegistration(Name("Test")), nullptr);

    // Re-registration overrides (no failure).
    EXPECT_TRUE(registry.Register(Name("Test"), MakeRegistration(2)));

    WorldPtr                         world = World::CreateWorld();
    std::unique_ptr<IWorldSubSystem> sub   = registry.Create(Name("Test"), *world, Any{});
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(static_cast<TestSubSystem *>(sub.get())->marker, 2);

    EXPECT_EQ(registry.Create(Name("Missing"), *world, Any{}), nullptr);
}

TEST_F(WorldSubSystemRegistryTest, BuildCreatesFromDescInOrder)
{
    auto &registry = WorldSubSystemRegistry::Get();
    registry.Register(Name("A"),
                      WorldSubSystemRegistration{
                          [](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> { return std::make_unique<OrderedSubSystem>(Name("A")); },
                          nullptr,
                          {},
                          {},
                      });
    registry.Register(Name("B"),
                      WorldSubSystemRegistration{
                          [](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> { return std::make_unique<OrderedSubSystem>(Name("B")); },
                          nullptr,
                          {},
                          {},
                      });

    std::vector<Name> order;
    gAttachOrder = &order;

    WorldDesc desc;
    desc.subSystems.push_back(WorldSubSystemDesc{Name("A"), Any{}, true});
    desc.subSystems.push_back(WorldSubSystemDesc{Name("Unknown"), Any{}, true}); // not registered
    desc.subSystems.push_back(WorldSubSystemDesc{Name("B"), Any{}, true});

    WorldPtr world = World::CreateWorld();
    world->Build(desc);

    EXPECT_NE(world->GetSubSystem(Name("A")), nullptr);
    EXPECT_NE(world->GetSubSystem(Name("B")), nullptr);
    EXPECT_EQ(world->GetSubSystem(Name("Unknown")), nullptr);

    ASSERT_EQ(order.size(), 2u);
    EXPECT_TRUE(order[0] == Name("A"));
    EXPECT_TRUE(order[1] == Name("B"));
}

TEST_F(WorldSubSystemRegistryTest, BuildSkipsDisabledAndDuplicate)
{
    auto &registry = WorldSubSystemRegistry::Get();
    registry.Register(Name("A"), MakeRegistration(1));

    WorldPtr world = World::CreateWorld();
    world->AddSubSystem(Name("A"), new TestSubSystem()); // pre-existing

    WorldDesc desc;
    desc.subSystems.push_back(WorldSubSystemDesc{Name("A"), Any{}, false}); // disabled
    desc.subSystems.push_back(WorldSubSystemDesc{Name("A"), Any{}, true});  // duplicate -> skip

    world->Build(desc); // must not assert/leak
    EXPECT_NE(world->GetSubSystem(Name("A")), nullptr);
}

TEST_F(WorldSubSystemRegistryTest, StartStopSimulationIteratesSubsystems)
{
    auto &registry = WorldSubSystemRegistry::Get();
    registry.Register(
        Name("A"), WorldSubSystemRegistration{
                       [](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> { return std::make_unique<SimSubSystem>(); }, nullptr, {}, {}});

    WorldDesc desc;
    desc.subSystems.push_back(WorldSubSystemDesc{Name("A"), Any{}, true});

    WorldPtr world = World::CreateWorld();
    world->Build(desc);
    world->StartSimulation();
    world->StopSimulation();

    auto *sub = static_cast<SimSubSystem *>(world->GetSubSystem(Name("A")));
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->starts, 1);
    EXPECT_EQ(sub->stops, 1);
}

TEST_F(WorldSubSystemRegistryTest, BuildValidatesConfig)
{
    auto &registry = WorldSubSystemRegistry::Get();
    registry.Register(Name("A"), WorldSubSystemRegistration{
                                     [](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> { return std::make_unique<TestSubSystem>(); },
                                     nullptr,
                                     {},
                                     [](const Any &, std::string &error) {
                                         error.clear();
                                         return true; // accept
                                     },
                                 });

    WorldDesc desc;
    desc.subSystems.push_back(WorldSubSystemDesc{Name("A"), Any{}, true});

    WorldPtr world = World::CreateWorld();
    world->Build(desc);
    EXPECT_NE(world->GetSubSystem(Name("A")), nullptr);
}
