#include <core/ecs/Dependency.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <random>

namespace dependency_test {
    struct Transform {};
    struct Bounds {};
} // namespace dependency_test
SKY_TYPE_TAG(dependency_test::Transform, "sky.test.dependency.Transform")
SKY_TYPE_TAG(dependency_test::Bounds, "sky.test.dependency.Bounds")

namespace {
    using namespace sky;

    SystemDesc System(SystemId id, std::vector<AccessDesc> accesses = {})
    {
        SystemDesc result;
        result.id       = id;
        result.name     = "System" + std::to_string(id);
        result.accesses = std::move(accesses);
        return result;
    }

    DependencyPlan Compile(const std::vector<SystemDesc> &systems)
    {
        auto result = CompileDependencyPlan(systems);
        if (auto *error = std::get_if<DependencyDiagnostic>(&result)) {
            ADD_FAILURE() << error->message;
            return {};
        }
        return std::get<DependencyPlan>(std::move(result));
    }

    void ExpectError(const std::vector<SystemDesc> &systems, DependencyErrorCode code)
    {
        auto result = CompileDependencyPlan(systems);
        ASSERT_TRUE(std::holds_alternative<DependencyDiagnostic>(result));
        const auto &error = std::get<DependencyDiagnostic>(result);
        EXPECT_EQ(error.code, code) << error.message;
        EXPECT_FALSE(error.message.empty());
    }

    bool HasEdge(const DependencyPlan &plan, SystemId from, SystemId to, DependencyOrigin origin)
    {
        return std::any_of(plan.edges.begin(), plan.edges.end(), [&](const auto &edge) {
            return plan.systems[edge.from].id == from && plan.systems[edge.to].id == to && edge.origin == origin;
        });
    }

    bool Reachable(const DependencyPlan &plan, uint32_t from, uint32_t to)
    {
        std::vector<bool> reached(plan.systems.size(), false);
        reached[from] = true;
        for (uint32_t node = from; node < to; ++node) {
            for (const auto &edge : plan.edges) {
                if (edge.from == node && reached[node]) {
                    reached[edge.to] = true;
                }
            }
        }
        return reached[to];
    }
} // namespace

TEST(EcsDependencyTest, EmptyPlanAndOwnedDescriptions)
{
    EXPECT_TRUE(Compile({}).systems.empty());
    std::vector<SystemDesc> input{System(1, {ResourceAccess("assets", {1, 1}, "mesh-metadata", AccessMode::READ)})};
    auto                    plan = Compile(input);
    input.clear();
    ASSERT_EQ(plan.systems.size(), 1u);
    EXPECT_EQ(plan.systems[0].name, "System1");
    EXPECT_EQ(plan.systems[0].accesses[0].tag, "mesh-metadata");
}

TEST(EcsDependencyTest, ReadReadAndDifferentOwnersDomainsGenerationsDoNotConflict)
{
    const auto read               = ComponentAccess<dependency_test::Transform>("scene", {1, 1}, AccessMode::READ);
    auto       otherOwner         = read;
    otherOwner.key.owner.instance = 2;
    otherOwner.mode               = AccessMode::WRITE;
    auto otherGeneration          = otherOwner;
    otherGeneration.key.owner     = {1, 2};
    auto otherDomain              = otherOwner;
    otherDomain.key.owner         = {1, 1};
    otherDomain.key.domain        = "view";
    const auto plan =
        Compile({System(1, {read}), System(2, {read}), System(3, {otherOwner}), System(4, {otherGeneration}), System(5, {otherDomain})});
    EXPECT_TRUE(plan.edges.empty());
}

TEST(EcsDependencyTest, ReadersJoinBeforeFollowingWriter)
{
    const auto read  = ComponentAccess<dependency_test::Transform>("scene", {1, 1}, AccessMode::READ);
    auto       write = read;
    write.mode       = AccessMode::WRITE;
    const auto plan  = Compile({System(1, {write}), System(2, {read}), System(3, {read}), System(4, {write})});
    EXPECT_TRUE(HasEdge(plan, 1, 2, DependencyOrigin::RAW));
    EXPECT_TRUE(HasEdge(plan, 1, 3, DependencyOrigin::RAW));
    EXPECT_TRUE(HasEdge(plan, 1, 4, DependencyOrigin::WAW));
    EXPECT_TRUE(HasEdge(plan, 2, 4, DependencyOrigin::WAR));
    EXPECT_TRUE(HasEdge(plan, 3, 4, DependencyOrigin::WAR));
    EXPECT_FALSE(Reachable(plan, 1, 2));
    for (const auto &edge : plan.edges) {
        ASSERT_TRUE(edge.access.has_value());
        EXPECT_EQ(edge.access.value(), read.key);
    }
}

TEST(EcsDependencyTest, SharedResourcesOrderOtherwiseIndependentComponents)
{
    const auto sharedRead  = ResourceAccess("shared", {10, 1}, "metadata-cache", AccessMode::READ);
    auto       sharedWrite = sharedRead;
    sharedWrite.mode       = AccessMode::WRITE;
    const auto plan        = Compile({System(1, {ComponentAccess<dependency_test::Transform>("scene", {1, 1}, AccessMode::WRITE), sharedRead}),
                                      System(2, {ComponentAccess<dependency_test::Bounds>("scene", {2, 1}, AccessMode::WRITE), sharedWrite})});
    ASSERT_EQ(plan.edges.size(), 1u);
    EXPECT_TRUE(HasEdge(plan, 1, 2, DependencyOrigin::WAR));
    EXPECT_EQ(plan.edges[0].access, sharedRead.key);
}

TEST(EcsDependencyTest, ExplicitOrderPrecedesHazardsAndDuplicateAccessesMerge)
{
    const auto read  = ComponentAccess<dependency_test::Transform>("scene", {1, 1}, AccessMode::READ);
    auto       write = read;
    write.mode       = AccessMode::WRITE;
    auto reader      = System(1, {read});
    auto writer      = System(2, {read, write, read, write});
    EXPECT_TRUE(HasEdge(Compile({reader, writer}), 1, 2, DependencyOrigin::WAR));
    reader.after    = {2, 2};
    writer.before   = {1};
    const auto plan = Compile({reader, writer});
    ASSERT_EQ(plan.systems.size(), 2u);
    EXPECT_EQ(plan.systems[0].id, 2u);
    ASSERT_EQ(plan.systems[0].accesses.size(), 1u);
    EXPECT_EQ(plan.systems[0].accesses[0].mode, AccessMode::WRITE);
    EXPECT_TRUE(HasEdge(plan, 2, 1, DependencyOrigin::RAW));
    EXPECT_TRUE(HasEdge(plan, 2, 1, DependencyOrigin::BEFORE));
    EXPECT_TRUE(HasEdge(plan, 2, 1, DependencyOrigin::AFTER));
    for (const auto &edge : plan.edges) {
        EXPECT_LT(edge.from, edge.to);
    }
}

TEST(EcsDependencyTest, PhasesAndStableOrderIgnoreRegistrationOrder)
{
    auto early   = System(10);
    auto later   = System(20);
    auto phase1  = System(1);
    auto phase2  = System(2);
    early.order  = -1;
    phase1.phase = 1;
    phase2.phase = 2;
    std::vector<SystemDesc> input{phase2, later, phase1, early};
    const auto              expected = Compile(input);
    ASSERT_EQ(expected.systems.size(), 4u);
    EXPECT_EQ(expected.systems[0].id, 10u);
    EXPECT_EQ(expected.systems[1].id, 20u);
    EXPECT_TRUE(HasEdge(expected, 10, 1, DependencyOrigin::PHASE));
    EXPECT_TRUE(HasEdge(expected, 20, 1, DependencyOrigin::PHASE));
    EXPECT_TRUE(HasEdge(expected, 1, 2, DependencyOrigin::PHASE));
    std::mt19937 random(42);
    for (int i = 0; i < 20; ++i) {
        std::shuffle(input.begin(), input.end(), random);
        EXPECT_EQ(Compile(input), expected);
    }
    early.after = {1};
    ExpectError({early, phase1}, DependencyErrorCode::PHASE_CONFLICT);
}

TEST(EcsDependencyTest, InvalidIdentityUnknownDependencyAndCollisionAreDiagnosed)
{
    ExpectError({System(0)}, DependencyErrorCode::INVALID_SYSTEM);
    ExpectError({System(1), System(1)}, DependencyErrorCode::DUPLICATE_SYSTEM);
    auto system   = System(1);
    system.before = {99};
    ExpectError({system}, DependencyErrorCode::UNKNOWN_SYSTEM);
    const auto access = ComponentAccess<dependency_test::Transform>("scene", {1, 1}, AccessMode::READ);
    for (int field = 0; field < 7; ++field) {
        auto invalid = access;
        switch (field) {
        case 0: invalid.key.domain.clear(); break;
        case 1: invalid.key.owner.instance = 0; break;
        case 2: invalid.key.owner.generation = 0; break;
        case 3: invalid.key.identity = 0; break;
        case 4: invalid.tag.clear(); break;
        case 5: invalid.mode = static_cast<AccessMode>(255); break;
        case 6: invalid.key.kind = static_cast<AccessKind>(255); break;
        }
        ExpectError({System(1, {invalid})}, DependencyErrorCode::INVALID_ACCESS);
    }
    auto collision               = access;
    collision.tag                = "different-type-with-same-id";
    collision.key.owner.instance = 2;
    auto result                  = CompileDependencyPlan(std::vector<SystemDesc>{System(1, {access}), System(2, {collision})});
    ASSERT_TRUE(std::holds_alternative<DependencyDiagnostic>(result));
    const auto &error = std::get<DependencyDiagnostic>(result);
    EXPECT_EQ(error.code, DependencyErrorCode::TAG_COLLISION);
    ASSERT_TRUE(error.access.has_value());
    EXPECT_EQ(error.access->identity, access.key.identity);
    EXPECT_NE(error.message.find(access.tag), std::string::npos);
    EXPECT_NE(error.message.find(collision.tag), std::string::npos);
}

TEST(EcsDependencyTest, CycleReportsActualMembersAndConstraintOrigins)
{
    auto a           = System(1);
    auto b           = System(2);
    auto downstream  = System(3);
    a.after          = {2};
    b.before         = {1};
    b.after          = {1};
    downstream.after = {2};
    auto result      = CompileDependencyPlan(std::vector<SystemDesc>{downstream, b, a});
    ASSERT_TRUE(std::holds_alternative<DependencyDiagnostic>(result));
    const auto &error = std::get<DependencyDiagnostic>(result);
    EXPECT_EQ(error.code, DependencyErrorCode::CYCLE);
    ASSERT_FALSE(error.edges.empty());
    for (const auto &edge : error.edges) {
        EXPECT_NE(edge.from, 3u);
        EXPECT_NE(edge.to, 3u);
        EXPECT_TRUE(edge.origin == DependencyOrigin::BEFORE || edge.origin == DependencyOrigin::AFTER);
    }
    EXPECT_NE(error.message.find("System1"), std::string::npos);
    EXPECT_NE(error.message.find("System2"), std::string::npos);
    a.after = {1};
    ExpectError({a}, DependencyErrorCode::CYCLE);
}

TEST(EcsDependencyTest, GeneratedConflictsAlwaysHaveDependencyPaths)
{
    std::mt19937 random(1234);
    for (int trial = 0; trial < 80; ++trial) {
        std::vector<SystemDesc> input;
        for (SystemId id = 1; id <= 16; ++id) {
            auto system = System(id);
            for (int access = 0; access < 4; ++access) {
                system.accesses.push_back(ResourceAccess("scene", {1 + random() % 3, 1}, "resource" + std::to_string(random() % 3),
                                                         random() % 2 ? AccessMode::READ : AccessMode::WRITE));
            }
            input.push_back(std::move(system));
        }
        const auto plan = Compile(input);
        for (uint32_t i = 0; i < plan.systems.size(); ++i) {
            for (uint32_t j = i + 1; j < plan.systems.size(); ++j) {
                for (const auto &a : plan.systems[i].accesses) {
                    for (const auto &b : plan.systems[j].accesses) {
                        if (a.key == b.key && (a.mode == AccessMode::WRITE || b.mode == AccessMode::WRITE)) {
                            ASSERT_TRUE(Reachable(plan, i, j));
                        }
                    }
                }
            }
        }
        std::shuffle(input.begin(), input.end(), random);
        EXPECT_EQ(Compile(input), plan);
    }
}
