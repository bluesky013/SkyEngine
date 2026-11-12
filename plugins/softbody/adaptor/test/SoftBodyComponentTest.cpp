#include <softbody/world/SoftBodyComponent.h>

#include "JoltBackend.h"
#include <animation/IAnimationPoseContributor.h>
#include <core/math/Matrix3.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/Interface.h>
#include <framework/world/TransformComponent.h>
#include <gtest/gtest.h>
#include <jolt/PhysicsSystem.h>
#include <physics/PhysicsBackendRegistry.h>
#include <physics/softbody/ISoftBodyWorldControl.h>
#include <softbody/assets/SoftBodyAsset.h>
#include <softbody/assets/SoftBodyRecipe.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>

using namespace sky;
using namespace sky::softbody;
using namespace sky::phy;

namespace {
    class DependencyAsset final : public AssetBase {
    public:
        const uint8_t *GetData() const override
        {
            return nullptr;
        }
    };
    class DependencyHandler final : public AssetHandlerBase {
    public:
        AssetPtr CreateAsset() override
        {
            return std::make_shared<DependencyAsset>();
        }
        bool Load(IStreamArchive &archive, const AssetPtr &) override
        {
            char value = 0;
            archive.GetStream().read(&value, 1);
            return archive.GetStream().good() && value == 42;
        }
        bool SaveChecked(OStreamArchive &, const AssetPtr &) override
        {
            return false;
        }
    };
    class AnimationService final : public IAnimationInstanceControl, public IAnimationPoseContributionRegistry {
    public:
        Uuid                                       skeleton;
        Uuid                                       generation = Uuid::Create();
        uint64_t                                   layout     = 7;
        bool                                       reject     = false;
        AnimationPoseContributionToken             token;
        std::shared_ptr<IAnimationPoseContributor> contributor;
        struct Registration {
            AnimationPoseContributionToken             token;
            std::shared_ptr<IAnimationPoseContributor> contributor;
        };
        std::vector<Registration> registrations;
        std::string_view          GetWorldSystemName() const override
        {
            return "Animation";
        }
        AnimationInstanceStatus Find(const World *world, const Uuid &actor) const override
        {
            AnimationInstanceStatus result;
            result.handle    = {world, world->GetRuntimeId(), actor, generation};
            const auto asset = AssetManager::Get()->FindAsset(skeleton);
            if (asset && asset->IsLoaded()) {
                result.state    = AnimationSourceState::Ready;
                result.skeleton = {skeleton, asset->GetInstanceId(), asset->GetRevision(), layout};
            }
            return result;
        }
        AnimationStatus Control(const AnimationInstanceHandle &, AnimationControlOp, float) override
        {
            return {AnimationError::Unsupported, {}};
        }
        AnimationStatus SetParameter(const AnimationInstanceHandle &, const Uuid &, const AnimationParam &) override
        {
            return {AnimationError::Unsupported, {}};
        }
        AnimationStatus RegisterPoseContributor(const AnimationPoseBinding                &binding,
                                                std::shared_ptr<IAnimationPoseContributor> value,
                                                AnimationPoseContributionToken            &out) override
        {
            if (reject || std::any_of(registrations.begin(), registrations.end(), [&](const auto &entry) {
                    return entry.token.binding.instance.world == binding.instance.world &&
                           entry.token.binding.instance.actor == binding.instance.actor;
                }))
                return {AnimationError::InvalidAsset, "Contribution rejected"};
            const auto current = Find(binding.instance.world, binding.instance.actor);
            if (binding.instance != current.handle || binding.skeleton != current.skeleton)
                return {AnimationError::StaleGeneration, {}};
            out = {Uuid::Create(), binding};
            registrations.push_back({out, std::move(value)});
            token       = registrations.front().token;
            contributor = registrations.front().contributor;
            return {};
        }
        AnimationStatus UnregisterPoseContributor(const AnimationPoseContributionToken &value) override
        {
            const auto found = std::find_if(registrations.begin(), registrations.end(),
                                            [&](const auto &entry) { return entry.token.id == value.id && entry.token.binding == value.binding; });
            if (found == registrations.end())
                return {AnimationError::StaleGeneration, {}};
            registrations.erase(found);
            token       = registrations.empty() ? AnimationPoseContributionToken{} : registrations.front().token;
            contributor = registrations.empty() ? nullptr : registrations.front().contributor;
            return {};
        }
    };

    class TestPhysicsSystem final : public IPhysicsSystem {
    public:
        std::unique_ptr<PhysicsSystem> implementation        = std::make_unique<PhysicsSystem>();
        uint32_t                       listenerRegistrations = 0;
        uint32_t                       listenerRemovals      = 0;
        bool                           Init()
        {
            return implementation->Init();
        }
        void Close()
        {
            implementation.reset();
        }
        IPhysicsWorld *GetWorld() const override
        {
            return implementation ? implementation->GetWorld() : nullptr;
        }
        void SetSimulating(bool value) override
        {
            implementation->SetSimulating(value);
        }
        bool IsSimulating() const override
        {
            return implementation && implementation->IsSimulating();
        }
        void SingleStep() override
        {
            implementation->SingleStep();
        }
        void Tick(float time) override
        {
            if (implementation)
                implementation->Tick(time);
        }
        void GatherCollisionMeshes(std::vector<CollisionMeshInstance> &out) const override
        {
            if (implementation)
                implementation->GatherCollisionMeshes(out);
        }
        PhysicsStatus AddStepListener(std::shared_ptr<IPhysicsStepListener> listener) override
        {
            const auto status = implementation->AddStepListener(std::move(listener));
            if (status)
                ++listenerRegistrations;
            return status;
        }
        void RemoveStepListener(const IPhysicsStepListener *listener) override
        {
            if (implementation) {
                implementation->RemoveStepListener(listener);
                ++listenerRemovals;
            }
        }
        float GetInterpolationAlpha() const override
        {
            return implementation ? implementation->GetInterpolationAlpha() : 1.f;
        }
        PhysicsWorldStats GetStats() const
        {
            return implementation->GetStats();
        }
        PhysicsOptions GetOptions() const
        {
            return implementation->GetOptions();
        }
        PhysicsStatus SetOptions(const PhysicsOptions &value)
        {
            return implementation->SetOptions(value);
        }
    };

    class SoftBodyComponentTest : public testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(PhysicsBackendRegistry::Get().Register(std::make_unique<JoltBackend>()));
            Interface<IAnimationInstanceControl>::Get()->Register(animation);
            Interface<IAnimationPoseContributionRegistry>::Get()->Register(animation);
            ASSERT_TRUE(StartSoftBodyWorldControl());
            std::error_code error;
            directory = std::filesystem::temp_directory_path(error) / ("sky-softbody-world-" + Uuid::Create().ToString());
            ASSERT_FALSE(error);
            products = new NativeFileSystem(FilePath(directory));
            bundle   = Uuid::Create().ToString();
            AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(products, bundle));
            data.mesh               = Uuid::Create();
            data.skin               = Uuid::Create();
            data.skeleton           = Uuid::Create();
            data.skeletonLayout     = 7;
            data.controlIds         = {"a", "b", "c", "d"};
            data.body.mesh.vertices = {{{0, 0, 0}}, {{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}};
            for (uint32_t i = 0; i < data.body.mesh.vertices.size(); ++i)
                data.body.rootFollow.push_back({i, .02f});
            data.body.mesh.edges      = {{{0, 1}}, {{0, 2}}, {{0, 3}}, {{1, 2}}, {{1, 3}}, {{2, 3}}};
            data.body.mesh.tetrahedra = {{{0, 1, 2, 3}}};
            data.body.mesh.faces      = {{1, 2, 3}, {0, 3, 2}, {0, 1, 3}, {0, 2, 1}};
            data.body.gravityFactor   = 0;
            animation.skeleton        = data.skeleton;
            Publish(rigId, data);
            world  = World::CreateWorld();
            system = new TestPhysicsSystem();
            ASSERT_TRUE(system->Init());
            world->AddSubSystem(Name(PHYSICS_SYSTEM_NAME.data()), system);
            system->SetSimulating(false);
            world->AddSubSystem(Name("Animation"), new IWorldSubSystem());
            actor     = world->CreateActor();
            component = actor->AddComponent<SoftBodyComponent>();
            ASSERT_NE(component, nullptr);
            ASSERT_TRUE(component->SetRig(rigId));
            Pump();
            SoftBodySnapshot snapshot;
            ASSERT_TRUE(component->GetSnapshot(snapshot)) << component->GetLastStatus().message;
        }
        void TearDown() override
        {
            world.Reset(nullptr);
            StopSoftBodyWorldControl();
            EXPECT_FALSE(animation.token.id);
            EXPECT_FALSE(animation.contributor);
            Interface<IAnimationPoseContributionRegistry>::Get()->UnRegister();
            Interface<IAnimationInstanceControl>::Get()->UnRegister();
            AssetExecutor::Get()->WaitForAll();
            EXPECT_TRUE(AssetManager::Get()->ResetRuntimeState());
            products.Reset(nullptr);
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error);
            PhysicsBackendRegistry::Get().Unregister();
        }
        void Publish(const Uuid &id, const SoftBodyAssetData &product)
        {
            std::vector<uint8_t> bytes;
            ASSERT_TRUE(EncodeSoftBodyAsset(product, bytes));
            const std::array<AssetProductData, 4> products{{{product.mesh, "AuroraMesh", {}, {42}},
                                                            {product.skin, "AuroraSkin", {}, {42}},
                                                            {product.skeleton, "AuroraSkeleton", {}, {42}},
                                                            {id, "SoftBody", {product.mesh, product.skin, product.skeleton}, std::move(bytes)}}};
            ASSERT_TRUE(AssetManager::Get()->SaveProductData(products, bundle));
        }
        void Pump()
        {
            for (unsigned i = 0; i < 8; ++i) {
                AssetExecutor::Get()->WaitForAll();
                world->Tick(0);
            }
        }
        WorldPtr              world;
        TestPhysicsSystem    *system    = nullptr;
        Actor                *actor     = nullptr;
        SoftBodyComponent    *component = nullptr;
        SoftBodyAssetData     data;
        Uuid                  rigId = Uuid::Create();
        AnimationService      animation;
        NativeFileSystemPtr   products;
        ProductBundleKey      bundle;
        std::filesystem::path directory;
    };
} // namespace

void RegisterSoftBodyWorldTestMetadata()
{
    RegisterSoftBodyAssets();
    SoftBodyComponent::Reflect(SerializationContext::Get());
    for (const auto *type : {"AuroraMesh", "AuroraSkin", "AuroraSkeleton"})
        AssetManager::Get()->RegisterAssetHandler(type, new DependencyHandler());
}

TEST_F(SoftBodyComponentTest, FixedStepFollowPauseResetAndOwnedSnapshots)
{
    SoftBodySnapshot before;
    ASSERT_TRUE(component->GetSnapshot(before));
    actor->GetComponent<TransformComponent>()->SetWorldTranslation({2, 0, 0});
    world->Tick(1.f);
    SoftBodySnapshot paused;
    ASSERT_TRUE(component->GetSnapshot(paused));
    EXPECT_EQ(paused.step, before.step);
    system->SingleStep();
    world->Tick(0);
    SoftBodySnapshot after;
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_EQ(after.step, before.step + 1);
    EXPECT_GT(after.worldPositions[0].x, 0.f);
    EXPECT_LT(after.worldPositions[0].x, 2.f);
    EXPECT_EQ(after.step, system->GetStats().stepCount);
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    EXPECT_FLOAT_EQ(before.worldPositions[0].x, 0.f);
    ASSERT_TRUE(component->Reset());
    SoftBodySnapshot reset;
    ASSERT_TRUE(component->GetSnapshot(reset));
    EXPECT_NEAR(reset.worldPositions[1].x, 3.f, 1e-5f);
    ASSERT_TRUE(animation.contributor);
    AnimationPoseBase base{animation.token.binding, 1, {Matrix4::Identity()}};
    for (const auto &v : data.body.mesh.vertices) {
        auto matrix = Matrix4::Identity();
        matrix[3]   = Vector4(v.position.x, v.position.y, v.position.z, 1);
        base.models.push_back(matrix);
    }
    const auto pose = animation.contributor->Evaluate(base);
    ASSERT_TRUE(pose.status) << pose.status.message;
    EXPECT_EQ(pose.models.size(), 5u);
    EXPECT_EQ(pose.modelNormals.size(), 5u);
}

TEST_F(SoftBodyComponentTest, DebugBodiesOwnTopologyAndMatchInterpolatedAnimationParticles)
{
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    SoftBodySnapshot before;
    ASSERT_TRUE(component->GetSnapshot(before));
    actor->GetComponent<TransformComponent>()->SetWorldTranslation({.2f, 0, 0});
    system->SetSimulating(true);
    world->Tick(1.5f / 60.f);
    SoftBodySnapshot committed;
    ASSERT_TRUE(component->GetSnapshot(committed));
    ASSERT_EQ(committed.step, before.step + 1);
    std::vector<SoftBodyDebugBody> bodies;
    ASSERT_TRUE(control->ReadDebugBodies(world->GetRuntimeId(), bodies));
    ASSERT_EQ(bodies.size(), 1u);
    const auto &body = bodies.front();
    EXPECT_EQ(body.id, committed.id);
    EXPECT_EQ(body.actor, actor->GetUuid());
    EXPECT_EQ(body.step, committed.step);
    EXPECT_EQ(body.revision, committed.revision);
    EXPECT_EQ(body.presentationStep, committed.step);
    EXPECT_FLOAT_EQ(body.vertexRadius, data.body.vertexRadius);
    EXPECT_EQ(body.committedWorldPositions, committed.worldPositions);
    ASSERT_EQ(body.edges.size(), data.body.mesh.edges.size());
    for (size_t i = 0; i < body.edges.size(); ++i)
        EXPECT_EQ(body.edges[i], data.body.mesh.edges[i].vertices);
    EXPECT_EQ(body.faces, data.body.mesh.faces);
    EXPECT_GT(body.interpolationAlpha, 0.f);
    EXPECT_LT(body.interpolationAlpha, 1.f);
    ASSERT_EQ(body.worldPositions.size(), before.worldPositions.size());
    AnimationPoseBase base{animation.token.binding, 1, {Matrix4::Identity()}};
    for (const auto &vertex : data.body.mesh.vertices) {
        auto model = Matrix4::Identity();
        model[3]   = Vector4(vertex.position.x, vertex.position.y, vertex.position.z, 1);
        base.models.push_back(model);
    }
    ASSERT_TRUE(animation.contributor);
    const auto pose = animation.contributor->Evaluate(base);
    ASSERT_TRUE(pose.status) << pose.status.message;
    EXPECT_EQ(body.presentationRevision, pose.revision);
    ASSERT_EQ(pose.models.size(), body.worldPositions.size() + 1);
    for (size_t i = 0; i < body.worldPositions.size(); ++i) {
        const auto expected = before.worldPositions[i] * (1 - body.interpolationAlpha) + committed.worldPositions[i] * body.interpolationAlpha;
        EXPECT_LT((body.worldPositions[i] - expected).Length(), 1e-6f);
        EXPECT_NEAR(body.worldPositions[i].x, pose.models[i + 1][3].x + .2f, 1e-6f);
        EXPECT_NEAR(body.worldPositions[i].y, pose.models[i + 1][3].y, 1e-6f);
        EXPECT_NEAR(body.worldPositions[i].z, pose.models[i + 1][3].z, 1e-6f);
    }
    const auto retained                         = body;
    bodies.front().worldPositions[0].x          = 91;
    bodies.front().committedWorldPositions[0].x = 92;
    bodies.front().edges[0]                     = {93, 94};
    bodies.front().faces.clear();
    ASSERT_TRUE(control->ReadDebugBodies(world->GetRuntimeId(), bodies));
    ASSERT_EQ(bodies.size(), 1u);
    EXPECT_EQ(bodies.front().worldPositions, retained.worldPositions);
    EXPECT_EQ(bodies.front().committedWorldPositions, retained.committedWorldPositions);
    EXPECT_EQ(bodies.front().edges, retained.edges);
    EXPECT_EQ(bodies.front().faces, retained.faces);
    EXPECT_EQ(bodies.front().presentationRevision, retained.presentationRevision);
}

TEST_F(SoftBodyComponentTest, DebugBodiesDistinguishCommittedFromLastPresentation)
{
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    std::vector<SoftBodyDebugBody> bodies;
    ASSERT_TRUE(control->ReadDebugBodies(world->GetRuntimeId(), bodies));
    ASSERT_EQ(bodies.size(), 1u);
    const auto before = bodies.front();
    actor->GetComponent<TransformComponent>()->SetWorldTranslation({.2f, 0, 0});
    system->SingleStep();
    SoftBodySnapshot committed;
    ASSERT_TRUE(component->GetSnapshot(committed));
    ASSERT_TRUE(control->ReadDebugBodies(world->GetRuntimeId(), bodies));
    ASSERT_EQ(bodies.size(), 1u);
    EXPECT_EQ(bodies.front().step, before.step + 1);
    EXPECT_EQ(bodies.front().committedWorldPositions, committed.worldPositions);
    EXPECT_EQ(bodies.front().presentationStep, before.presentationStep);
    EXPECT_EQ(bodies.front().presentationRevision, before.presentationRevision);
    EXPECT_EQ(bodies.front().worldPositions, before.worldPositions);
    world->Tick(0);
    ASSERT_TRUE(control->ReadDebugBodies(world->GetRuntimeId(), bodies));
    EXPECT_EQ(bodies.front().presentationStep, committed.step);
    EXPECT_GT(bodies.front().presentationRevision, before.presentationRevision);
}

TEST_F(SoftBodyComponentTest, DebugBodiesExcludeDisabledMembersAndPreserveOutputForUnavailableSessions)
{
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    const auto                     epoch = world->GetRuntimeId();
    std::vector<SoftBodyDebugBody> bodies;
    ASSERT_TRUE(control->ReadDebugBodies(epoch, bodies));
    ASSERT_EQ(bodies.size(), 1u);
    const auto retained = bodies.front();
    EXPECT_FALSE(control->ReadDebugBodies(Uuid::Create(), bodies));
    ASSERT_EQ(bodies.size(), 1u);
    EXPECT_EQ(bodies.front().id, retained.id);
    EXPECT_EQ(bodies.front().worldPositions, retained.worldPositions);
    ASSERT_TRUE(component->SetRig({}));
    ASSERT_TRUE(control->ReadDebugBodies(epoch, bodies));
    EXPECT_TRUE(bodies.empty());
    bodies.push_back(retained);
    system->Close();
    EXPECT_FALSE(control->ReadDebugBodies(epoch, bodies));
    ASSERT_EQ(bodies.size(), 1u);
    EXPECT_EQ(bodies.front().id, retained.id);
    EXPECT_EQ(bodies.front().committedWorldPositions, retained.committedWorldPositions);
    EXPECT_EQ(bodies.front().edges, retained.edges);
}

TEST_F(SoftBodyComponentTest, MissingReplacementRetainsBodyAndClearingThenReloadingWorks)
{
    SoftBodySnapshot before;
    ASSERT_TRUE(component->GetSnapshot(before));
    ASSERT_TRUE(component->SetRig(Uuid::Create()));
    Pump();
    EXPECT_FALSE(component->GetLastStatus());
    SoftBodySnapshot after;
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_EQ(after.id, before.id);
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    ASSERT_TRUE(component->SetRig({}));
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    EXPECT_FALSE(animation.token.id);
    ASSERT_TRUE(component->SetRig(rigId));
    Pump();
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_NE(after.id, before.id);
    EXPECT_TRUE(animation.token.id);
}

TEST_F(SoftBodyComponentTest, InvalidReplacementInitializationKeepsPreviousInstanceAndSnapshot)
{
    SoftBodySnapshot before;
    ASSERT_TRUE(component->GetSnapshot(before));
    actor->GetComponent<TransformComponent>()->SetWorldScale({2, 1, 1});
    const auto replacement = Uuid::Create();
    Publish(replacement, data);
    ASSERT_TRUE(component->SetRig(replacement));
    Pump();
    EXPECT_FALSE(component->GetLastStatus());
    SoftBodySnapshot after;
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_EQ(after.id, before.id);
    EXPECT_EQ(after.worldPositions, before.worldPositions);
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    EXPECT_TRUE(animation.token.id);
    actor->GetComponent<TransformComponent>()->SetWorldScale({1, 1, 1});
    Pump();
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_NE(after.id, before.id);
    EXPECT_TRUE(component->GetLastStatus());
}

TEST_F(SoftBodyComponentTest, ClosingWorldRevokesContributionAndInvalidatesUiReads)
{
    const auto retained = animation.contributor;
    const auto binding  = animation.token.binding;
    system->Close();
    EXPECT_FALSE(animation.token.id);
    SoftBodySnapshot untouched;
    untouched.step = 91;
    EXPECT_FALSE(component->GetSnapshot(untouched));
    EXPECT_EQ(untouched.step, 91u);
    EXPECT_FALSE(component->Reset());
    EXPECT_FALSE(retained->Evaluate({binding, 1, {}}).status);
    world->Tick(0);
}

TEST_F(SoftBodyComponentTest, FailedBeforeStepPreservesSnapshotAndResetClearsDiagnostic)
{
    SoftBodySnapshot before;
    ASSERT_TRUE(component->GetSnapshot(before));
    actor->GetComponent<TransformComponent>()->SetWorldScale({2, 1, 1});
    system->SingleStep();
    EXPECT_FALSE(component->GetLastStatus());
    SoftBodySnapshot after;
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_EQ(after.step, before.step);
    EXPECT_EQ(after.worldPositions, before.worldPositions);
    actor->GetComponent<TransformComponent>()->SetWorldScale({1, 1, 1});
    ASSERT_TRUE(component->Reset());
    EXPECT_TRUE(component->GetLastStatus());
}

TEST_F(SoftBodyComponentTest, DynamicRootFollowReceivesActorTargetWithoutKinematicControls)
{
    data.body.rootFollow.clear();
    for (uint32_t i = 0; i < data.body.mesh.vertices.size(); ++i) {
        data.body.mesh.vertices[i].mode        = SoftBodyAttachmentMode::Dynamic;
        data.body.mesh.vertices[i].inverseMass = 1.f;
        data.body.rootFollow.push_back({i, .02f});
    }
    data.body.solverIterations = 1;
    data.body.linearDamping    = 0;
    const auto dynamicRig      = Uuid::Create();
    Publish(dynamicRig, data);
    ASSERT_TRUE(component->SetRig(dynamicRig));
    Pump();
    SoftBodySnapshot before;
    ASSERT_TRUE(component->GetSnapshot(before)) << component->GetLastStatus().message;
    actor->GetComponent<TransformComponent>()->SetWorldTranslation({1, 0, 0});
    system->SingleStep();
    world->Tick(0);
    SoftBodySnapshot after;
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_EQ(after.step, before.step + 1);
    ASSERT_EQ(after.worldPositions.size(), data.body.mesh.vertices.size());
    constexpr float delta        = 1.f / 60.f;
    const float     displacement = 1.f / (1.f + .02f / (delta * delta));
    for (size_t i = 0; i < after.worldPositions.size(); ++i) {
        EXPECT_NEAR(after.worldPositions[i].x - before.worldPositions[i].x, displacement, 2e-5f);
        EXPECT_GT(after.worldPositions[i].x, before.worldPositions[i].x);
        EXPECT_LT(after.worldPositions[i].x, before.worldPositions[i].x + 1.f);
        EXPECT_NEAR(after.worldVelocities[i].x, displacement / delta, 2e-4f);
    }
    EXPECT_EQ(after.step, system->GetStats().stepCount);
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
}

TEST_F(SoftBodyComponentTest, CpuParticlesCollideWithStaticJoltGroundWithoutRegisteringBodies)
{
    PhysicsBodyDesc ground;
    ground.kind                  = BodyKind::Static;
    ground.shape.halfExt         = {10, .1f, 10};
    ground.transform.translation = {0, -.1f, 0};
    const auto groundId          = system->GetWorld()->CreateBody(ground);
    ASSERT_TRUE(IsValid(groundId));
    ASSERT_EQ(system->GetStats().bodyCount, 1u);
    data.body.rootFollow.clear();
    data.body.gravityFactor    = 1.f;
    data.body.vertexRadius     = .05f;
    data.body.solverIterations = 4;
    for (auto &vertex : data.body.mesh.vertices) {
        vertex.position.y += .5f;
        vertex.velocity = {0, -50, 0};
    }
    const auto fallingRig = Uuid::Create();
    Publish(fallingRig, data);
    ASSERT_TRUE(component->SetRig(fallingRig));
    Pump();
    ASSERT_TRUE(component->GetLastStatus()) << component->GetLastStatus().message;
    system->SingleStep();
    ASSERT_TRUE(component->GetLastStatus()) << component->GetLastStatus().message;
    SoftBodySnapshot snapshot;
    ASSERT_TRUE(component->GetSnapshot(snapshot));
    for (const auto &position : snapshot.worldPositions)
        EXPECT_GE(position.y, data.body.vertexRadius - .001f);
    EXPECT_LT(snapshot.worldPositions[0].y, .5f);
    EXPECT_EQ(snapshot.step, system->GetStats().stepCount);
    EXPECT_EQ(system->GetStats().bodyCount, 1u);
    Transform unchanged;
    ASSERT_TRUE(system->GetWorld()->GetBodyTransform(groundId, unchanged));
    EXPECT_EQ(unchanged.translation, ground.transform.translation);
}

TEST_F(SoftBodyComponentTest, ReplacementUsesCompletedRigidStepInsteadOfLocalCounter)
{
    for (uint32_t i = 0; i < 5; ++i)
        system->SingleStep();
    const auto replacement = Uuid::Create();
    Publish(replacement, data);
    ASSERT_TRUE(component->SetRig(replacement));
    Pump();
    SoftBodySnapshot before;
    ASSERT_TRUE(component->GetSnapshot(before));
    EXPECT_EQ(before.step, 0u);
    system->SingleStep();
    ASSERT_TRUE(component->GetLastStatus()) << component->GetLastStatus().message;
    SoftBodySnapshot after;
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_EQ(after.step, 6u);
    EXPECT_EQ(after.step, system->GetStats().stepCount);
}

TEST_F(SoftBodyComponentTest, LiveParametersApplyAtomicallyToSelectedInstancesOfOneRig)
{
    auto *otherActor = world->CreateActor();
    auto *other      = otherActor->AddComponent<SoftBodyComponent>();
    ASSERT_NE(other, nullptr);
    ASSERT_TRUE(other->SetRig(rigId));
    Pump();
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    const auto                             epoch = world->GetRuntimeId();
    std::vector<SoftBodyRuntimeParameters> rows;
    ASSERT_TRUE(control->ReadBodyParameters(epoch, rows));
    ASSERT_EQ(rows.size(), 2u);
    ASSERT_LT(rows[0].actor, rows[1].actor);
    const std::array ids{rows[0].id, rows[1].id};
    for (const auto &row : rows) {
        EXPECT_EQ(row.controlPoints, 4u);
        EXPECT_EQ(row.solverIterations, data.body.solverIterations);
        EXPECT_EQ(row.pressure, 0.f);
        EXPECT_EQ(row.followCompliance.count, 4u);
        EXPECT_EQ(row.edgeCompliance.count, 6u);
        EXPECT_EQ(row.volumeCompliance.count, 1u);
    }
    SoftBodyParameterPatch patch;
    patch.edgeCompliance = .01f;
    ASSERT_TRUE(control->ApplyBodyParameters(epoch, std::span(ids).first(1), patch));
    ASSERT_TRUE(control->ReadBodyParameters(epoch, rows));
    EXPECT_EQ(rows[0].edgeCompliance.minimum, .01f);
    EXPECT_EQ(rows[1].edgeCompliance.minimum, data.body.mesh.edges.front().compliance);
    patch.edgeCompliance   = .03f;
    patch.volumeCompliance = 2e-16f;
    patch.followCompliance = .04f;
    patch.linearDamping    = -1.f;
    EXPECT_FALSE(control->ApplyBodyParameters(epoch, ids, patch));
    patch.linearDamping = 2.f;
    const std::array stale{ids[0], Uuid::Create()};
    EXPECT_FALSE(control->ApplyBodyParameters(epoch, stale, patch));
    ASSERT_TRUE(control->ReadBodyParameters(epoch, rows));
    EXPECT_EQ(rows[0].edgeCompliance.maximum, .01f);
    EXPECT_EQ(rows[0].linearDamping, data.body.linearDamping);
    ASSERT_TRUE(control->ApplyBodyParameters(epoch, ids, patch));
    ASSERT_TRUE(control->ReadBodyParameters(epoch, rows));
    for (const auto &row : rows) {
        EXPECT_EQ(row.volumeCompliance.minimum, 2e-16f);
        EXPECT_EQ(row.linearDamping, 2.f);
        EXPECT_EQ(row.followCompliance.minimum, .04f);
        EXPECT_EQ(row.edgeCompliance.maximum, .03f);
    }
    const auto asset = AssetManager::Get()->FindAsset<SoftBodyAsset>(rigId);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->Data().body.linearDamping, data.body.linearDamping);
    EXPECT_EQ(asset->Data().body.mesh.edges.front().compliance, data.body.mesh.edges.front().compliance);
}

TEST_F(SoftBodyComponentTest, WorldCoordinatorPublishesBothBodiesWithOneListenerAndEverySubstepSample)
{
    auto *otherActor = world->CreateActor();
    otherActor->GetComponent<TransformComponent>()->SetWorldTranslation({3, 0, 0});
    auto *other = otherActor->AddComponent<SoftBodyComponent>();
    ASSERT_NE(other, nullptr);
    ASSERT_TRUE(other->SetRig(rigId));
    Pump();
    SoftBodySnapshot before, otherBefore;
    ASSERT_TRUE(component->GetSnapshot(before));
    ASSERT_TRUE(other->GetSnapshot(otherBefore));
    EXPECT_EQ(system->listenerRegistrations, 1u);
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    SoftBodyStepMetrics stats;
    ASSERT_TRUE(control->GetStats(world->GetRuntimeId(), stats));
    EXPECT_EQ(stats.bodyCount, 2u);
    SoftBodySimulationSettings settings;
    settings.rigidCollision = false;
    settings.softCollision  = true;
    settings.profileSteps   = true;
    ASSERT_TRUE(control->Configure(world->GetRuntimeId(), settings));
    system->SetSimulating(true);
    world->Tick(3.1f / 60.f);
    SoftBodySnapshot after, otherAfter;
    ASSERT_TRUE(component->GetSnapshot(after));
    ASSERT_TRUE(other->GetSnapshot(otherAfter));
    EXPECT_EQ(after.step, otherAfter.step);
    EXPECT_GE(after.step - before.step, 3u);
    std::vector<SoftBodyStepMetrics> samples;
    ASSERT_TRUE(control->ReadSamples(world->GetRuntimeId(), before.step, samples));
    ASSERT_EQ(samples.size(), after.step - before.step);
    for (size_t i = 0; i < samples.size(); ++i) {
        EXPECT_EQ(samples[i].step, before.step + i + 1);
        EXPECT_TRUE(samples[i].committed);
        EXPECT_EQ(samples[i].bodyCount, 2u);
        EXPECT_EQ(samples[i].particles, 8u);
        EXPECT_EQ(samples[i].rigidQueries, 0u);
    }
    otherActor->RemoveComponent<SoftBodyComponent>();
    ASSERT_TRUE(control->GetStats(world->GetRuntimeId(), stats));
    EXPECT_EQ(stats.bodyCount, 1u);
    EXPECT_EQ(system->listenerRemovals, 0u);
    actor->RemoveComponent<SoftBodyComponent>();
    component       = nullptr;
    stats.bodyCount = 91;
    EXPECT_FALSE(control->GetStats(world->GetRuntimeId(), stats));
    EXPECT_EQ(stats.bodyCount, 91u);
    EXPECT_EQ(system->listenerRemovals, 1u);
}

TEST_F(SoftBodyComponentTest, FailedBatchRetainsEveryBodyAndProfilesTheFailedAttempt)
{
    auto *otherActor = world->CreateActor();
    otherActor->GetComponent<TransformComponent>()->SetWorldTranslation({3, 0, 0});
    auto *other = otherActor->AddComponent<SoftBodyComponent>();
    ASSERT_NE(other, nullptr);
    ASSERT_TRUE(other->SetRig(rigId));
    Pump();
    SoftBodySnapshot before, otherBefore;
    ASSERT_TRUE(component->GetSnapshot(before));
    ASSERT_TRUE(other->GetSnapshot(otherBefore));
    PhysicsBodyDesc obstacle;
    obstacle.kind                  = BodyKind::Dynamic;
    obstacle.gravityEnabled        = false;
    obstacle.shape.halfExt         = {.1f, .1f, .1f};
    obstacle.transform.translation = {3, 0, 0};
    ASSERT_TRUE(IsValid(system->GetWorld()->CreateBody(obstacle)));
    auto                      *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    SoftBodySimulationSettings settings;
    settings.profileSteps = true;
    ASSERT_TRUE(control->Configure(world->GetRuntimeId(), settings));
    actor->GetComponent<TransformComponent>()->SetWorldTranslation({.02f, 0, 0});
    system->SingleStep();
    EXPECT_FALSE(component->GetLastStatus());
    EXPECT_FALSE(other->GetLastStatus());
    SoftBodySnapshot after, otherAfter;
    ASSERT_TRUE(component->GetSnapshot(after));
    ASSERT_TRUE(other->GetSnapshot(otherAfter));
    EXPECT_EQ(after.step, before.step);
    EXPECT_EQ(after.revision, before.revision);
    EXPECT_EQ(after.worldPositions, before.worldPositions);
    EXPECT_EQ(otherAfter.step, otherBefore.step);
    EXPECT_EQ(otherAfter.revision, otherBefore.revision);
    EXPECT_EQ(otherAfter.worldPositions, otherBefore.worldPositions);
    std::vector<SoftBodyStepMetrics> samples;
    ASSERT_TRUE(control->ReadSamples(world->GetRuntimeId(), 0, samples));
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_FALSE(samples.front().committed);
    EXPECT_EQ(samples.front().failure, PhysicsError::Unsupported);
    EXPECT_EQ(samples.front().bodyCount, 2u);
    EXPECT_EQ(samples.front().completedSteps, 0u);
    std::vector<SoftBodyDebugBody> debug;
    ASSERT_TRUE(control->ReadDebugBodies(world->GetRuntimeId(), debug));
    ASSERT_EQ(debug.size(), 2u);
    EXPECT_LT(debug[0].actor, debug[1].actor);
    for (const auto &body : debug) {
        const auto &snapshot = body.actor == actor->GetUuid() ? before : otherBefore;
        EXPECT_EQ(body.id, snapshot.id);
        EXPECT_EQ(body.step, snapshot.step);
        EXPECT_EQ(body.committedWorldPositions, snapshot.worldPositions);
        EXPECT_EQ(body.worldPositions, snapshot.worldPositions);
    }
}

TEST_F(SoftBodyComponentTest, ProfilingHistoryExpiresExplicitlyAndDisabledProfilingKeepsClockFieldsZero)
{
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    SoftBodySimulationSettings settings;
    settings.rigidCollision = false;
    settings.profileSteps   = true;
    ASSERT_TRUE(control->Configure(world->GetRuntimeId(), settings));
    for (uint32_t i = 0; i < 4097; ++i)
        system->SingleStep();
    ASSERT_TRUE(component->GetLastStatus()) << component->GetLastStatus().message;
    std::vector<SoftBodyStepMetrics> samples(1);
    samples.front().step = 99999;
    EXPECT_EQ(control->ReadSamples(world->GetRuntimeId(), 0, samples).error, PhysicsError::CapacityExceeded);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples.front().step, 99999u);
    ASSERT_TRUE(control->ReadSamples(world->GetRuntimeId(), 1, samples));
    ASSERT_EQ(samples.size(), 4096u);
    EXPECT_EQ(samples.front().step, 2u);
    EXPECT_EQ(samples.back().step, 4097u);
    settings.profileSteps = false;
    ASSERT_TRUE(control->Configure(world->GetRuntimeId(), settings));
    system->SingleStep();
    SoftBodyStepMetrics stats;
    ASSERT_TRUE(control->GetStats(world->GetRuntimeId(), stats));
    EXPECT_EQ(stats.step, 4098u);
    EXPECT_EQ(stats.totalNs, 0u);
    EXPECT_EQ(stats.predictNs, 0u);
    EXPECT_EQ(stats.internalXpbdNs, 0u);
    EXPECT_EQ(stats.rigidQueryNs, 0u);
    ASSERT_TRUE(control->ReadSamples(world->GetRuntimeId(), 4097, samples));
    EXPECT_TRUE(samples.empty());
}

TEST_F(SoftBodyComponentTest, WorldControlRejectsClosedSessionsAndRevokesHeldPoseOnServiceShutdown)
{
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    EXPECT_FALSE(StartSoftBodyWorldControl());
    SoftBodyStepMetrics untouched;
    untouched.step = 77;
    EXPECT_FALSE(control->GetStats(Uuid::Create(), untouched));
    EXPECT_EQ(untouched.step, 77u);
    SoftBodySimulationSettings invalid;
    invalid.softContactCompliance = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(control->Configure(world->GetRuntimeId(), invalid).error, PhysicsError::InvalidArgument);
    const auto retained = animation.contributor;
    const auto binding  = animation.token.binding;
    const auto epoch    = world->GetRuntimeId();
    StopSoftBodyWorldControl();
    EXPECT_EQ(Interface<ISoftBodyWorldControl>::Get()->GetApi(), nullptr);
    EXPECT_FALSE(control->GetStats(epoch, untouched));
    EXPECT_EQ(untouched.step, 77u);
    EXPECT_FALSE(retained->Evaluate({binding, 1, {}}).status);
    EXPECT_FALSE(component->Reset());
    EXPECT_EQ(system->listenerRemovals, 1u);
    EXPECT_TRUE(StartSoftBodyWorldControl());
    EXPECT_FALSE(Interface<ISoftBodyWorldControl>::Get()->GetApi()->GetStats(epoch, untouched));
}

TEST_F(SoftBodyComponentTest, FailedFirstMemberRegistrationLeavesNoListenerOrWorldSession)
{
    const WorldPtr otherWorld  = World::CreateWorld();
    auto          *otherSystem = new TestPhysicsSystem();
    ASSERT_TRUE(otherSystem->Init());
    otherWorld->AddSubSystem(Name(PHYSICS_SYSTEM_NAME.data()), otherSystem);
    otherWorld->AddSubSystem(Name("Animation"), new IWorldSubSystem());
    Actor invalid(Uuid{});
    ASSERT_NE(invalid.AddComponent<TransformComponent>(), nullptr);
    ASSERT_NE(invalid.AddComponent<SoftBodyComponent>(), nullptr);
    std::string error;
    EXPECT_FALSE(invalid.AttachToWorld(otherWorld.Get(), &error, false));
    EXPECT_EQ(invalid.GetWorld(), nullptr);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(otherSystem->listenerRegistrations, 1u);
    EXPECT_EQ(otherSystem->listenerRemovals, 1u);
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    SoftBodyStepMetrics untouched;
    untouched.step = 77;
    EXPECT_FALSE(control->GetStats(otherWorld->GetRuntimeId(), untouched));
    EXPECT_EQ(untouched.step, 77u);
    auto *replacement = otherWorld->CreateActor();
    ASSERT_NE(replacement->AddComponent<SoftBodyComponent>(), nullptr);
    EXPECT_EQ(otherSystem->listenerRegistrations, 2u);
    EXPECT_TRUE(control->GetStats(otherWorld->GetRuntimeId(), untouched));
    replacement->RemoveComponent<SoftBodyComponent>();
    EXPECT_EQ(otherSystem->listenerRemovals, 2u);
    EXPECT_FALSE(control->GetStats(otherWorld->GetRuntimeId(), untouched));
}

TEST_F(SoftBodyComponentTest, DuplicateMemberRegistrationPreservesExistingWorldAndSimulation)
{
    const auto       registrations = system->listenerRegistrations;
    const auto       removals      = system->listenerRemovals;
    SoftBodySnapshot before;
    ASSERT_TRUE(component->GetSnapshot(before));
    Actor duplicate(actor->GetUuid());
    ASSERT_NE(duplicate.AddComponent<TransformComponent>(), nullptr);
    ASSERT_NE(duplicate.AddComponent<SoftBodyComponent>(), nullptr);
    std::string error;
    EXPECT_FALSE(duplicate.AttachToWorld(world.Get(), &error, false));
    EXPECT_EQ(duplicate.GetWorld(), nullptr);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(system->listenerRegistrations, registrations);
    EXPECT_EQ(system->listenerRemovals, removals);
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    SoftBodyStepMetrics stats;
    ASSERT_TRUE(control->GetStats(world->GetRuntimeId(), stats));
    EXPECT_EQ(stats.bodyCount, 1u);
    system->SingleStep();
    SoftBodySnapshot after;
    ASSERT_TRUE(component->GetSnapshot(after));
    EXPECT_EQ(after.id, before.id);
    EXPECT_GT(after.step, before.step);
}

TEST_F(SoftBodyComponentTest, IndependentWorldSessionsKeepConfigurationSamplesAndOwnershipSeparate)
{
    CounterPtr<World> otherWorld  = World::CreateWorld();
    auto             *otherSystem = new TestPhysicsSystem();
    ASSERT_TRUE(otherSystem->Init());
    otherWorld->AddSubSystem(Name(PHYSICS_SYSTEM_NAME.data()), otherSystem);
    otherWorld->AddSubSystem(Name("Animation"), new IWorldSubSystem());
    auto *otherActor = otherWorld->CreateActor();
    auto *other      = otherActor->AddComponent<SoftBodyComponent>();
    ASSERT_NE(other, nullptr);
    ASSERT_TRUE(other->SetRig(rigId));
    for (unsigned i = 0; i < 8; ++i) {
        AssetExecutor::Get()->WaitForAll();
        otherWorld->Tick(0);
    }
    SoftBodySnapshot snapshot;
    ASSERT_TRUE(other->GetSnapshot(snapshot));
    auto                      *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    SoftBodySimulationSettings settings;
    settings.rigidCollision = false;
    settings.profileSteps   = true;
    ASSERT_TRUE(control->Configure(world->GetRuntimeId(), settings));
    system->SingleStep();
    otherSystem->SingleStep();
    std::vector<SoftBodyStepMetrics> samples;
    ASSERT_TRUE(control->ReadSamples(world->GetRuntimeId(), 0, samples));
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples.front().bodyCount, 1u);
    ASSERT_TRUE(control->ReadSamples(otherWorld->GetRuntimeId(), 0, samples));
    EXPECT_TRUE(samples.empty());
    EXPECT_EQ(system->listenerRegistrations, 1u);
    EXPECT_EQ(otherSystem->listenerRegistrations, 1u);
    const auto otherEpoch = otherWorld->GetRuntimeId();
    otherWorld.Reset(nullptr);
    SoftBodyStepMetrics stats;
    ASSERT_TRUE(control->GetStats(world->GetRuntimeId(), stats));
    EXPECT_EQ(stats.bodyCount, 1u);
    EXPECT_FALSE(control->GetStats(otherEpoch, stats));
    EXPECT_EQ(stats.bodyCount, 1u);
}

TEST_F(SoftBodyComponentTest, OriginalSuppliedRecipeSettlesAndMovesWithStandardSkinningAtSixtyHertz)
{
    const char *path = std::getenv("SKY_SOFTBODY_SUPPLIED_RECIPE");
    if (!path)
        GTEST_SKIP() << "Optional original supplied-cage recipe";
    std::ifstream stream(path, std::ios::binary);
    ASSERT_TRUE(stream.good());
    const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    SoftBodyRecipe    recipe;
    const auto        parsed = ParseSoftBodyRecipe(text, recipe);
    ASSERT_TRUE(parsed) << parsed.message;
    ASSERT_EQ(recipe.settings.generator.mode, RigGenerationMode::SuppliedCenterFan);
    ASSERT_EQ(recipe.settings.normalPolicy, SoftBodyNormalPolicy::StandardSkinning);
    ASSERT_EQ(recipe.controls.size(), 15u);
    ASSERT_EQ(recipe.settings.generator.centerIndex, 14u);
    ASSERT_EQ(recipe.settings.generator.tetrahedra.size(), 24u);
    ASSERT_FLOAT_EQ(recipe.settings.material.particleMass, 1.f);
    ASSERT_FLOAT_EQ(recipe.settings.material.edgeCompliance, .002f);
    ASSERT_FLOAT_EQ(recipe.settings.material.followCompliance, .02f);
    ASSERT_FLOAT_EQ(recipe.settings.material.vertexRadius, .02f);
    ASSERT_FLOAT_EQ(recipe.settings.material.volumeCompliance, 2e-16f);
    ASSERT_FLOAT_EQ(recipe.settings.material.linearDamping, 2.f);
    ASSERT_EQ(recipe.settings.material.solverIterations, 4u);
    SoftBodyMesh mesh;
    data.controlIds.clear();
    float minimumY = INFINITY;
    for (const auto &control : recipe.controls) {
        ASSERT_EQ(control.mode, SoftBodyAttachmentMode::Dynamic);
        mesh.vertices.push_back({control.position, VEC3_ZERO, 1.f, control.mode});
        data.controlIds.push_back(control.id);
        minimumY = std::min(minimumY, control.position.y);
    }
    struct FaceUse {
        std::array<uint32_t, 3> face{};
        uint32_t                count = 0;
    };
    std::map<std::array<uint32_t, 3>, FaceUse> faces;
    std::set<std::array<uint32_t, 2>>          edges;
    for (const auto &source : recipe.settings.generator.tetrahedra) {
        const std::array<uint32_t, 4> tet{source.a, source.b, source.c, source.d};
        mesh.tetrahedra.push_back({tet, 0.f});
        for (size_t i = 0; i < 4; ++i) {
            std::array<uint32_t, 3> face{};
            size_t                  cursor = 0;
            for (size_t j = 0; j < 4; ++j) {
                if (i < j)
                    edges.insert({std::min(tet[i], tet[j]), std::max(tet[i], tet[j])});
                if (i != j)
                    face[cursor++] = tet[j];
            }
            const auto a = mesh.vertices[face[0]].position;
            if ((mesh.vertices[face[1]].position - a).Cross(mesh.vertices[face[2]].position - a).Dot(mesh.vertices[tet[i]].position - a) > 0)
                std::swap(face[1], face[2]);
            auto key = face;
            std::sort(key.begin(), key.end());
            auto &use = faces[key];
            use.face  = face;
            ++use.count;
        }
    }
    for (const auto &[key, use] : faces) {
        (void)key;
        ASSERT_LE(use.count, 2u);
        if (use.count == 1)
            mesh.faces.push_back(use.face);
    }
    for (const auto &edge : edges)
        mesh.edges.push_back({edge, 0.f});
    ASSERT_EQ(mesh.faces.size(), 24u);
    ASSERT_EQ(mesh.edges.size(), 50u);
    const auto configured = ConfigureRigPhysics(recipe.settings.material, mesh, data.body);
    ASSERT_TRUE(configured) << configured.message;
    data.normalPolicy = recipe.settings.normalPolicy;
    PhysicsBodyDesc ground;
    ground.kind                  = BodyKind::Static;
    ground.shape.halfExt         = {3.f, .1f, 3.f};
    ground.transform.translation = {0.f, -.1f, 0.f};
    ASSERT_TRUE(IsValid(system->GetWorld()->CreateBody(ground)));
    auto *transform = actor->GetComponent<TransformComponent>();
    ASSERT_NE(transform, nullptr);
    const float rootY = recipe.settings.material.vertexRadius - minimumY + .05f;
    transform->SetWorldTranslation({0, rootY, 0});
    const auto suppliedRig = Uuid::Create();
    Publish(suppliedRig, data);
    ASSERT_TRUE(component->SetRig(suppliedRig));
    Pump();
    ASSERT_TRUE(component->GetLastStatus()) << component->GetLastStatus().message;
    SoftBodySnapshot snapshot;
    ASSERT_TRUE(component->GetSnapshot(snapshot));
    const auto identity     = snapshot.id;
    uint64_t   previousStep = snapshot.step, contactSteps = 0;
    auto      *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    SoftBodySimulationSettings settings;
    settings.profileSteps = true;
    ASSERT_TRUE(control->Configure(world->GetRuntimeId(), settings));
    ASSERT_FALSE(recipe.settings.attachments.empty());
    const auto              anchor   = recipe.settings.attachments.front().bindingAnchor;
    const auto              embedded = anchor * recipe.settings.generator.bindingPositionScale;
    std::array<uint32_t, 4> anchorBones{};
    std::array<float, 4>    anchorWeights{};
    bool                    found = false;
    for (const auto &tet : mesh.tetrahedra) {
        const auto                &v = tet.vertices;
        const auto                 a = mesh.vertices[v[0]].position;
        const Matrix3              basis(mesh.vertices[v[1]].position - a, mesh.vertices[v[2]].position - a, mesh.vertices[v[3]].position - a);
        const auto                 w = basis.Inverse() * (embedded - a);
        const std::array<float, 4> weights{1.f - w.x - w.y - w.z, w.x, w.y, w.z};
        if (*std::min_element(weights.begin(), weights.end()) < -1e-6f)
            continue;
        anchorBones   = v;
        anchorWeights = weights;
        found         = true;
        break;
    }
    ASSERT_TRUE(found);
    float minimumVolumeRatio = INFINITY;
    for (uint32_t frame = 0; frame < 360; ++frame) {
        SCOPED_TRACE(frame + 1);
        transform->SetWorldTranslation({frame < 180 ? 0.f : .02f * static_cast<float>(frame - 179) / 60.f, rootY, 0});
        system->SingleStep();
        world->Tick(0);
        SoftBodyStepMetrics stats;
        ASSERT_TRUE(control->GetStats(world->GetRuntimeId(), stats));
        ASSERT_TRUE(stats.committed) << component->GetLastStatus().message << " internal=" << stats.internalIterations
                                     << " queries=" << stats.rigidQueries << " active=" << stats.activeRigidContacts
                                     << " predict_ns=" << stats.predictNs << " internal_ns=" << stats.internalXpbdNs;
        ASSERT_TRUE(component->GetSnapshot(snapshot));
        ASSERT_EQ(snapshot.id, identity);
        ASSERT_EQ(snapshot.step, ++previousStep);
        ASSERT_EQ(snapshot.step, system->GetStats().stepCount);
        contactSteps += stats.activeRigidContacts != 0;
        for (const auto &tet : mesh.tetrahedra) {
            const auto &v = tet.vertices;
            const auto  p = snapshot.worldPositions[v[0]], r = mesh.vertices[v[0]].position;
            const float volume6 =
                (snapshot.worldPositions[v[1]] - p).Dot((snapshot.worldPositions[v[2]] - p).Cross(snapshot.worldPositions[v[3]] - p));
            const float rest6 = (mesh.vertices[v[1]].position - r).Dot((mesh.vertices[v[2]].position - r).Cross(mesh.vertices[v[3]].position - r));
            ASSERT_GT(volume6, 0.f);
            minimumVolumeRatio = std::min(minimumVolumeRatio, volume6 / rest6);
        }
        for (const auto &face : mesh.faces)
            for (const auto index : face)
                ASSERT_GE(snapshot.worldPositions[index].y, recipe.settings.material.vertexRadius - .0001f);
        ASSERT_TRUE(animation.contributor);
        AnimationPoseBase base{animation.token.binding, frame + 1, {Matrix4::Identity()}};
        for (const auto &point : mesh.vertices) {
            auto model = Matrix4::Identity();
            model[3]   = Vector4(point.position.x, point.position.y, point.position.z, 1);
            base.models.push_back(model);
        }
        const auto pose = animation.contributor->Evaluate(base);
        ASSERT_TRUE(pose.status) << pose.status.message;
        ASSERT_EQ(pose.models.size(), 16u);
        ASSERT_TRUE(pose.modelNormals.empty());
        Vector3 expected = transform->GetWorldTransform().Translate(anchor), skinned{};
        for (size_t k = 0; k < 4; ++k) {
            const uint32_t particle = anchorBones[k];
            const auto     rest     = mesh.vertices[particle].position;
            expected += (snapshot.worldPositions[particle] - transform->GetWorldTransform().Translate(rest)) * anchorWeights[k];
            const auto local = anchor - rest;
            const auto moved = pose.models[particle + 1] * Vector4(local.x, local.y, local.z, 1);
            skinned += Vector3{moved.x, moved.y, moved.z} * anchorWeights[k];
        }
        ASSERT_LT((transform->GetWorldTransform().Translate(skinned) - expected).Length(), 2e-6f);
    }
    EXPECT_GT(contactSteps, 30u);
    EXPECT_EQ(system->GetStats().bodyCount, 1u);
    std::cout << "Supplied recipe 60Hz steps=360 gravity=-9.81 root_speed=.02 contact_steps=" << contactSteps
              << " min_volume_ratio=" << minimumVolumeRatio << " custom_normals=0\n";
}

TEST_F(SoftBodyComponentTest, AuthoredSealSettlesOnJoltGroundThroughFourWeightBinding)
{
    const char *path = std::getenv("SKY_SOFTBODY_SEAL_FIXTURE");
    if (!path)
        GTEST_SKIP() << "Optional external original seal fixture";
    std::ifstream stream(path);
    ASSERT_TRUE(stream.good());
    uint32_t vertexCount = 0, triangleCount = 0;
    stream >> vertexCount >> triangleCount;
    ASSERT_GT(vertexCount, 0u);
    ASSERT_LT(vertexCount, 100000u);
    ASSERT_GT(triangleCount, 0u);
    ASSERT_LT(triangleCount, 200000u);

    RigGenerationInput input;
    input.source.key                 = "static";
    input.bodyPrimitive              = "static/primitive/0";
    input.settings.maxControlPoints  = 30;
    input.settings.useExplicitCenter = true;
    input.settings.cageCenter        = {-.10f, .12f, 0.f};
    input.controls                   = {{"root-anchor", {0.f, .20f, 0.f}, SoftBodyAttachmentMode::Dynamic},
                                        {"head-anchor", {.25f, .28f, 0.f}, SoftBodyAttachmentMode::Dynamic}};
    auto &primitive                  = input.source.primitives.emplace_back();
    primitive.key                    = input.bodyPrimitive;
    primitive.positions.resize(vertexCount);
    primitive.indices.resize(triangleCount * 3);
    float sourceMinY = std::numeric_limits<float>::max();
    for (auto &position : primitive.positions) {
        stream >> position.x >> position.y >> position.z;
        sourceMinY = std::min(sourceMinY, position.y);
    }
    for (auto &index : primitive.indices) {
        stream >> index;
        ASSERT_LT(index, vertexCount);
    }
    ASSERT_TRUE(stream.good());
    RigGenerationOutput rig;
    const auto          generated = GenerateRig(input, rig);
    ASSERT_TRUE(generated) << generated.message;
    ASSERT_LE(rig.mesh.vertices.size(), 30u);
    ASSERT_EQ(rig.binding.primitives.size(), 1u);
    const auto &weights = rig.binding.primitives[0].vertices;
    ASSERT_EQ(weights.size(), primitive.positions.size());
    ASSERT_EQ(rig.binding.bones.size(), rig.mesh.vertices.size() + 1);
    for (size_t i = 0; i < weights.size(); ++i) {
        Vector3 rest{};
        float   sum = 0.f;
        for (size_t slot = 0; slot < 4; ++slot) {
            ASSERT_GT(weights[i].bones[slot], 0u);
            ASSERT_LT(weights[i].bones[slot], rig.binding.bones.size());
            ASSERT_GE(weights[i].weights[slot], 0.f);
            sum += weights[i].weights[slot];
            rest += rig.mesh.vertices[weights[i].bones[slot] - 1].position * weights[i].weights[slot];
        }
        ASSERT_NEAR(sum, 1.f, 2e-7f);
        ASSERT_LT((rest - primitive.positions[i]).Length(), 1e-6f);
    }

    RigMaterialSettings material;
    material.particleMass     = .1f;
    material.vertexRadius     = .005f;
    material.edgeCompliance   = 1e-5f;
    material.volumeCompliance = 1e-9f;
    material.enableRootFollow = true;
    material.followCompliance = .02f;
    material.linearDamping    = .1f;
    material.solverIterations = 4;
    const auto configured     = ConfigureRigPhysics(material, rig.mesh, data.body);
    ASSERT_TRUE(configured) << configured.message;
    data.controlIds     = rig.controlIds;
    data.skeletonLayout = rig.binding.layoutSignature;
    animation.layout    = data.skeletonLayout;
    ASSERT_EQ(data.body.rootFollow.size(), rig.mesh.vertices.size());
    for (const auto &vertex : data.body.mesh.vertices) {
        ASSERT_EQ(vertex.mode, SoftBodyAttachmentMode::Dynamic);
        ASSERT_FLOAT_EQ(vertex.inverseMass, 10.f);
    }

    PhysicsBodyDesc ground;
    ground.kind                  = BodyKind::Static;
    ground.shape.halfExt         = {3.f, .1f, 3.f};
    ground.transform.translation = {0.f, -.1f, 0.f};
    const auto groundId          = system->GetWorld()->CreateBody(ground);
    ASSERT_TRUE(IsValid(groundId));
    auto *transform = actor->GetComponent<TransformComponent>();
    ASSERT_NE(transform, nullptr);
    constexpr float startRootY   = .2f;
    const float     restingRootY = material.vertexRadius - sourceMinY;
    transform->SetWorldTranslation({0.f, startRootY, 0.f});
    const auto sealRig = Uuid::Create();
    Publish(sealRig, data);
    ASSERT_TRUE(component->SetRig(sealRig));
    Pump();
    ASSERT_TRUE(component->GetLastStatus()) << component->GetLastStatus().message;
    SoftBodySnapshot snapshot;
    ASSERT_TRUE(component->GetSnapshot(snapshot));
    const auto instance     = snapshot.id;
    uint64_t   previousStep = snapshot.step;
    float      settledGap   = std::numeric_limits<float>::max();

    for (uint32_t frame = 0; frame < 240; ++frame) {
        SCOPED_TRACE(frame);
        float rootY = restingRootY;
        if (frame < 90)
            rootY = startRootY + (restingRootY - startRootY) * static_cast<float>(frame + 1) / 90.f;
        else if (frame >= 150 && frame < 180)
            rootY -= .01f * static_cast<float>(frame - 149) / 30.f;
        else if (frame >= 180 && frame < 210)
            rootY -= .01f * (1.f - static_cast<float>(frame - 179) / 30.f);
        transform->SetWorldTranslation({0.f, rootY, 0.f});
        system->SingleStep();
        world->Tick(0);
        ASSERT_TRUE(component->GetLastStatus()) << component->GetLastStatus().message;
        ASSERT_TRUE(component->GetSnapshot(snapshot));
        ASSERT_EQ(snapshot.id, instance);
        ASSERT_EQ(snapshot.step, previousStep + 1);
        ASSERT_EQ(snapshot.step, system->GetStats().stepCount);
        ASSERT_EQ(snapshot.worldPositions.size(), rig.mesh.vertices.size());
        previousStep = snapshot.step;

        ASSERT_TRUE(animation.contributor);
        AnimationPoseBase base;
        base.binding  = animation.token.binding;
        base.revision = frame + 1;
        for (const auto &bone : rig.binding.bones)
            base.models.push_back(bone.reference.ToMatrix());
        const auto pose = animation.contributor->Evaluate(base);
        ASSERT_TRUE(pose.status) << pose.status.message;
        ASSERT_EQ(pose.models.size(), rig.binding.bones.size());
        ASSERT_EQ(pose.modelNormals.size(), rig.binding.bones.size());
        for (const auto &normal : pose.modelNormals) {
            for (const float value : normal.v)
                ASSERT_TRUE(std::isfinite(value));
            ASSERT_GT(std::abs(normal.Determinant()), 1e-7f);
        }
        float visibleMinY = std::numeric_limits<float>::max();
        for (size_t i = 0; i < weights.size(); ++i) {
            Vector3 visible = primitive.positions[i];
            Vector3 posed{};
            for (size_t slot = 0; slot < 4; ++slot) {
                const auto  bone   = weights[i].bones[slot];
                const float weight = weights[i].weights[slot];
                const auto &rest   = rig.mesh.vertices[bone - 1].position;
                visible += (snapshot.worldPositions[bone - 1] - rest) * weight;
                const auto local       = primitive.positions[i] - rig.binding.bones[bone].reference.translation;
                const auto transformed = pose.models[bone] * Vector4(local.x, local.y, local.z, 1.f);
                posed += Vector3(transformed.x, transformed.y, transformed.z) * weight;
            }
            posed = transform->GetWorldTransform().Translate(posed);
            ASSERT_LT((posed - visible).Length(), 2e-6f);
            ASSERT_TRUE(std::isfinite(visible.x) && std::isfinite(visible.y) && std::isfinite(visible.z));
            visibleMinY = std::min(visibleMinY, visible.y);
        }
        if (frame == 149 || frame == 179 || frame == 239)
            std::cout << "Seal landing frame=" << frame + 1 << " controls=" << rig.mesh.vertices.size() << " tets=" << rig.mesh.tetrahedra.size()
                      << " root_y=" << rootY << " visible_min_y=" << visibleMinY << " support_gap=" << rig.diagnostics.axisSupportGaps[2] << '\n';
        ASSERT_GE(visibleMinY, -.001f);
        if (frame == 179)
            EXPECT_GE(visibleMinY - (rootY + sourceMinY), .003f);
        if (frame == 149) {
            EXPECT_LE(visibleMinY, .02f);
            settledGap = visibleMinY;
        } else if (frame == 239) {
            EXPECT_LE(visibleMinY, .02f);
            EXPECT_NEAR(visibleMinY, settledGap, .01f);
        }
    }
    EXPECT_EQ(system->GetStats().bodyCount, 1u);
    Transform unchanged;
    ASSERT_TRUE(system->GetWorld()->GetBodyTransform(groundId, unchanged));
    EXPECT_EQ(unchanged.translation, ground.transform.translation);
}

TEST_F(SoftBodyComponentTest, AuthoredSealPairMaintainsValidBoundPosesDuringSlowGroundedContact)
{
    const char *path = std::getenv("SKY_SOFTBODY_SEAL_FIXTURE");
    if (!path)
        GTEST_SKIP() << "Optional external original seal fixture";
    std::ifstream stream(path);
    ASSERT_TRUE(stream.good());
    uint32_t vertexCount = 0, triangleCount = 0;
    stream >> vertexCount >> triangleCount;
    ASSERT_GT(vertexCount, 0u);
    ASSERT_LT(vertexCount, 100000u);
    ASSERT_GT(triangleCount, 0u);
    ASSERT_LT(triangleCount, 200000u);
    RigGenerationInput input;
    input.source.key                 = "static";
    input.bodyPrimitive              = "static/primitive/0";
    input.settings.maxControlPoints  = 30;
    input.settings.useExplicitCenter = true;
    input.settings.cageCenter        = {-.10f, .12f, 0.f};
    const char *budgetText           = std::getenv("SKY_SOFTBODY_SEAL_BUDGET");
    if (budgetText) {
        const std::string_view text(budgetText);
        const auto             parsed = std::from_chars(text.data(), text.data() + text.size(), input.settings.maxControlPoints);
        ASSERT_EQ(parsed.ec, std::errc{});
        ASSERT_EQ(parsed.ptr, text.data() + text.size());
        input.settings.fillControlBudget  = true;
        input.settings.maxVolumeExcess    = 10.f;
        input.settings.maxSurfaceDistance = 5.f;
    }
    float approachSpeed = .00375f;
    if (const char *speedText = std::getenv("SKY_SOFTBODY_PAIR_SPEED")) {
        const std::string_view text(speedText);
        const auto             parsed = std::from_chars(text.data(), text.data() + text.size(), approachSpeed);
        ASSERT_EQ(parsed.ec, std::errc{});
        ASSERT_EQ(parsed.ptr, text.data() + text.size());
    }
    ASSERT_TRUE(std::isfinite(approachSpeed));
    ASSERT_GT(approachSpeed, 0.f);
    ASSERT_LE(approachSpeed, .2f);
    input.controls  = {{"root-anchor", {0.f, .20f, 0.f}, SoftBodyAttachmentMode::Dynamic},
                       {"head-anchor", {.25f, .28f, 0.f}, SoftBodyAttachmentMode::Dynamic}};
    auto &primitive = input.source.primitives.emplace_back();
    primitive.key   = input.bodyPrimitive;
    primitive.positions.resize(vertexCount);
    primitive.indices.resize(triangleCount * 3);
    float minY = std::numeric_limits<float>::max();
    float minZ = minY, maxZ = -minY;
    for (auto &position : primitive.positions) {
        stream >> position.x >> position.y >> position.z;
        minY = std::min(minY, position.y);
        minZ = std::min(minZ, position.z);
        maxZ = std::max(maxZ, position.z);
    }
    for (auto &index : primitive.indices) {
        stream >> index;
        ASSERT_LT(index, vertexCount);
    }
    ASSERT_TRUE(stream.good());
    RigGenerationOutput rig;
    const auto          generated = GenerateRig(input, rig);
    ASSERT_TRUE(generated) << generated.message;
    ASSERT_LE(rig.mesh.vertices.size(), 30u);
    ASSERT_EQ(rig.binding.primitives.size(), 1u);
    const auto &weights = rig.binding.primitives.front().vertices;
    ASSERT_EQ(weights.size(), vertexCount);
    if (budgetText)
        ASSERT_EQ(rig.mesh.vertices.size(), input.settings.maxControlPoints);
    minY = minZ = std::numeric_limits<float>::max();
    maxZ        = -minZ;
    for (const auto &face : rig.mesh.faces)
        for (const auto index : face) {
            const auto &position = rig.mesh.vertices[index].position;
            minY                 = std::min(minY, position.y);
            minZ                 = std::min(minZ, position.z);
            maxZ                 = std::max(maxZ, position.z);
        }

    RigMaterialSettings material;
    material.particleMass     = .1f;
    material.vertexRadius     = .005f;
    material.edgeCompliance   = 1e-5f;
    material.volumeCompliance = 1e-9f;
    material.enableRootFollow = true;
    material.followCompliance = .02f;
    material.linearDamping    = .1f;
    material.solverIterations = 4;
    if (budgetText) {
        material.particleMass     = 3.f / static_cast<float>(rig.mesh.vertices.size());
        material.followCompliance = .02f * static_cast<float>(rig.mesh.vertices.size()) / 30.f;
    }
    ASSERT_TRUE(ConfigureRigPhysics(material, rig.mesh, data.body));
    data.controlIds     = rig.controlIds;
    data.skeletonLayout = rig.binding.layoutSignature;
    animation.layout    = data.skeletonLayout;
    const auto sealRig  = Uuid::Create();
    Publish(sealRig, data);

    PhysicsBodyDesc ground;
    ground.kind                  = BodyKind::Static;
    ground.shape.halfExt         = {3.f, .1f, 3.f};
    ground.transform.translation = {0.f, -.1f, 0.f};
    ASSERT_TRUE(IsValid(system->GetWorld()->CreateBody(ground)));
    const float width       = maxZ - minZ;
    float       initialLift = 0.f;
    if (const char *liftText = std::getenv("SKY_SOFTBODY_PAIR_LIFT")) {
        const std::string_view text(liftText);
        const auto             parsed = std::from_chars(text.data(), text.data() + text.size(), initialLift);
        ASSERT_EQ(parsed.ec, std::errc{});
        ASSERT_EQ(parsed.ptr, text.data() + text.size());
    }
    ASSERT_TRUE(std::isfinite(initialLift));
    ASSERT_GE(initialLift, 0.f);
    const float rootY          = material.vertexRadius - minY + .004f + initialLift;
    const float startSpacing   = width + 2.f * material.vertexRadius + .01f;
    auto       *firstTransform = actor->GetComponent<TransformComponent>();
    ASSERT_NE(firstTransform, nullptr);
    firstTransform->SetWorldTranslation({0.f, rootY, -startSpacing * .5f});
    ASSERT_TRUE(component->SetRig(sealRig));
    Pump();
    auto *otherActor      = world->CreateActor();
    auto *secondTransform = otherActor->GetComponent<TransformComponent>();
    ASSERT_NE(secondTransform, nullptr);
    secondTransform->SetWorldTranslation({0.f, rootY, startSpacing * .5f});
    auto *other = otherActor->AddComponent<SoftBodyComponent>();
    ASSERT_NE(other, nullptr);
    ASSERT_TRUE(other->SetRig(sealRig));
    Pump();
    const std::array<SoftBodyComponent *, 2>  components{component, other};
    const std::array<Actor *, 2>              actors{actor, otherActor};
    const std::array<TransformComponent *, 2> transforms{firstTransform, secondTransform};
    std::array<Uuid, 2>                       identities;
    for (size_t i = 0; i < components.size(); ++i) {
        SoftBodySnapshot snapshot;
        ASSERT_TRUE(components[i]->GetSnapshot(snapshot));
        identities[i] = snapshot.id;
    }
    auto *control = Interface<ISoftBodyWorldControl>::Get()->GetApi();
    ASSERT_NE(control, nullptr);
    SoftBodySimulationSettings settings;
    settings.softCollision  = true;
    settings.profileSteps   = true;
    settings.rigidCollision = std::getenv("SKY_SOFTBODY_PAIR_NO_RIGID") == nullptr;
    ASSERT_TRUE(control->Configure(world->GetRuntimeId(), settings));
    uint64_t activeContacts = 0, contactSteps = 0;
    for (uint32_t frame = 0; frame < 210; ++frame) {
        SCOPED_TRACE(frame);
        const float approach = std::clamp((static_cast<float>(frame) - 59.f) * approachSpeed / (60.f * .0075f), 0.f, 1.f);
        const float spacing  = startSpacing - .015f * approach;
        firstTransform->SetWorldTranslation({0.f, rootY, -spacing * .5f});
        secondTransform->SetWorldTranslation({0.f, rootY, spacing * .5f});
        system->SingleStep();
        world->Tick(0);
        SoftBodyStepMetrics stats;
        ASSERT_TRUE(control->GetStats(world->GetRuntimeId(), stats));
        ASSERT_TRUE(stats.committed) << components[0]->GetLastStatus().message << " / " << components[1]->GetLastStatus().message
                                     << " predictNs=" << stats.predictNs << " internalNs=" << stats.internalXpbdNs
                                     << " rigidQueries=" << stats.rigidQueries << " softContacts=" << stats.activeSoftContacts
                                     << " internalPasses=" << stats.internalIterations;
        ASSERT_EQ(stats.completedSteps, 1u);
        ASSERT_EQ(stats.bodyCount, 2u);
        activeContacts += stats.activeSoftContacts;
        contactSteps += stats.activeSoftContacts != 0;
        for (size_t i = 0; i < components.size(); ++i) {
            ASSERT_TRUE(components[i]->GetLastStatus()) << components[i]->GetLastStatus().message;
            SoftBodySnapshot snapshot;
            ASSERT_TRUE(components[i]->GetSnapshot(snapshot));
            ASSERT_EQ(snapshot.id, identities[i]);
            ASSERT_EQ(snapshot.step, system->GetStats().stepCount);
            for (const auto &tet : rig.mesh.tetrahedra) {
                const auto &v = tet.vertices;
                const auto  a = snapshot.worldPositions[v[1]] - snapshot.worldPositions[v[0]];
                const auto  b = snapshot.worldPositions[v[2]] - snapshot.worldPositions[v[0]];
                const auto  c = snapshot.worldPositions[v[3]] - snapshot.worldPositions[v[0]];
                ASSERT_GT(a.Dot(b.Cross(c)), 0.f);
            }
            const auto registration = std::find_if(animation.registrations.begin(), animation.registrations.end(),
                                                   [&](const auto &entry) { return entry.token.binding.instance.actor == actors[i]->GetUuid(); });
            ASSERT_NE(registration, animation.registrations.end());
            AnimationPoseBase base;
            base.binding  = registration->token.binding;
            base.revision = frame + 1;
            for (const auto &bone : rig.binding.bones)
                base.models.push_back(bone.reference.ToMatrix());
            const auto pose = registration->contributor->Evaluate(base);
            ASSERT_TRUE(pose.status) << pose.status.message;
            ASSERT_EQ(pose.models.size(), rig.binding.bones.size());
            ASSERT_EQ(pose.modelNormals.size(), rig.binding.bones.size());
            for (const auto &normal : pose.modelNormals)
                for (const auto value : normal.v)
                    ASSERT_TRUE(std::isfinite(value));
            for (size_t vertex = 0; vertex < weights.size(); vertex += 173) {
                Vector3 posed{}, expected{};
                for (size_t slot = 0; slot < 4; ++slot) {
                    const auto bone = weights[vertex].bones[slot];
                    ASSERT_GT(bone, 0u);
                    ASSERT_LT(bone, rig.binding.bones.size());
                    const auto weight      = weights[vertex].weights[slot];
                    const auto local       = primitive.positions[vertex] - rig.binding.bones[bone].reference.translation;
                    const auto transformed = pose.models[bone] * Vector4(local.x, local.y, local.z, 1.f);
                    posed += Vector3(transformed.x, transformed.y, transformed.z) * weight;
                    expected += snapshot.worldPositions[bone - 1] * weight;
                }
                posed = transforms[i]->GetWorldTransform().Translate(posed);
                ASSERT_LT((posed - expected).Length(), 3e-6f);
            }
        }
    }
    std::cout << "Seal pair controls=" << rig.mesh.vertices.size() << " speed=" << approachSpeed << " steps=210 active_contacts=" << activeContacts
              << " contact_steps=" << contactSteps << '\n';
    EXPECT_GT(activeContacts, 0u);
    EXPECT_GT(contactSteps, 10u);
}
