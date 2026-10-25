#include <animation/IAnimationClipSource.h>
#include <animation/IAnimationSkeletonSource.h>
#include <animation/assets/GraphAsset.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/world/AnimationComponent.h>
#include <animation/world/AnimationWorldHost.h>
#include <array>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <framework/interface/Interface.h>
#include <framework/world/TransformComponent.h>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>

using namespace sky;
using namespace sky::animation;
using namespace sky::aurora;

namespace {
    template <class T>
    struct Record {
        AnimationSourceResult<T> value;
    };
    template <class T>
    class Operation final : public IAnimationSourceRequest<T> {
    public:
        explicit Operation(std::shared_ptr<Record<T>> value) : record(std::move(value))
        {
        }
        AnimationSourceResult<T> Poll() override
        {
            return record->value;
        }
        std::shared_ptr<Record<T>> record;
    };
    template <class T, class API>
    class Source final : public API {
    public:
        std::shared_ptr<const void>                          code = std::make_shared<int>(1);
        std::unordered_map<Uuid, std::shared_ptr<Record<T>>> records;
        uint64_t                                             serial = 0;
        uint64_t                                             GetChangeSerial() const override
        {
            return serial;
        }
        AnimationSourceRequest<T> Request(const Uuid &id) override
        {
            const auto found = records.find(id);
            if (found == records.end()) {
                return {};
            }
            return {code, std::make_shared<Operation<T>>(found->second)};
        }
        std::shared_ptr<Record<T>> Publish(const Uuid &id, CounterPtr<T> resource, uint64_t layout = 0)
        {
            auto next               = std::make_shared<Record<T>>();
            next->value.resource    = std::move(resource);
            next->value.moduleLease = code;
            next->value.state       = AnimationSourceState::Ready;
            next->value.version     = {id, Uuid::Create(), 1, layout};
            records[id]             = next;
            return next;
        }
    };
    AnimClipPtr Clip(float distance = 4.f)
    {
        AnimClipPtr clip = new AnimationClip(Name("walk"));
        clip->SetFrameRate(1.f);
        clip->SetNumFrame(4);
        AnimNodeChannelData channel;
        channel.name           = "root";
        channel.position.times = {0, 3};
        channel.position.keys  = {Vector3(0.f), Vector3(distance, 0.f, 0.f)};
        EXPECT_TRUE(clip->AddChannel(new AnimationNodeChannel(channel)));
        return clip;
    }
    class AnimationWorldTest : public testing::Test {
    protected:
        void SetUp() override
        {
            Interface<IAnimationClipSource>::Get()->Register(clips);
            Interface<IAnimationSkeletonSource>::Get()->Register(rigs);
            SkeletonData data;
            data.boneData = {{Name("root"), INVALID_BONE_ID}, {Name("root/helper"), 0}};
            data.refPos.resize(2);
            data.refPos[1].translation = Vector3(0.f, 2.f, 0.f);
            rig                        = Skeleton::BuildSkeleton(data);
            rigs.Publish(rigId, rig, rig->GetLayoutSignature());
            clips.Publish(clipId, Clip());
            host  = CreateAnimationWorldHost(std::make_shared<int>(1));
            world = World::CreateWorld();
            world->AddSubSystem(Name(ANIMATION_WORLD_SYSTEM), host->CreateSystem().release());
        }
        void TearDown() override
        {
            world.Reset(nullptr);
            host.reset();
            Interface<IAnimationClipSource>::Get()->UnRegister();
            Interface<IAnimationSkeletonSource>::Get()->UnRegister();
        }
        Actor *Add(bool autoplay = true)
        {
            auto                  *actor     = world->CreateActor();
            auto                  *component = actor->AddComponent<AnimationComponent>();
            AnimationComponentData config;
            config.skeleton = rigId;
            config.clip     = clipId;
            config.autoplay = autoplay;
            component->SetConfig(config);
            return actor;
        }
        Source<AnimationClip, IAnimationClipSource> clips;
        Source<Skeleton, IAnimationSkeletonSource>  rigs;
        Uuid                                        rigId  = Uuid::Create();
        Uuid                                        clipId = Uuid::Create();
        SkeletonPtr                                 rig;
        std::unique_ptr<AnimationWorldHost>         host;
        WorldPtr                                    world;
    };
} // namespace

namespace {
    class AnimationGraphWorldTest : public AnimationWorldTest {
    protected:
        void SetUp() override
        {
            AnimationWorldTest::SetUp();
            std::error_code error;
            directory = std::filesystem::temp_directory_path(error) / ("sky-graph-world-" + Uuid::Create().ToString());
            ASSERT_FALSE(error);
            products = new NativeFileSystem(FilePath(directory));
            bundle   = graphId.ToString();
            AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(products, bundle));
            graph.document   = graphId;
            graph.skeleton   = rigId;
            graph.parameters = {{parameter, "Weight", GraphParameterType::Float, 0.f}};
            const auto idle  = Uuid::Create();
            clips.Publish(idle, Clip(0.f));
            GraphNode a, b, blend, output;
            a.id                 = Uuid::Create();
            a.clip               = clipId;
            b.id                 = Uuid::Create();
            b.clip               = idle;
            blend.id             = Uuid::Create();
            blend.kind           = GraphNodeKind::Blend2;
            blend.alphaParameter = parameter;
            output.id            = Uuid::Create();
            output.kind          = GraphNodeKind::Output;
            graph.nodes          = {a, b, blend, output};
            graph.links          = {{b.id, GraphPin::Pose, blend.id, GraphPin::InputA},
                                    {a.id, GraphPin::Pose, blend.id, GraphPin::InputB},
                                    {blend.id, GraphPin::Pose, output.id, GraphPin::InputA}};
            graph.output         = output.id;
            Publish();
        }
        void TearDown() override
        {
            AnimationWorldTest::TearDown();
            AssetExecutor::Get()->WaitForAll();
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error);
        }
        void Publish()
        {
            auto      *manager = AssetManager::Get();
            const auto asset   = std::static_pointer_cast<Asset<GraphAsset>>(manager->CreateBuildAsset(graphId, Name("AnimationGraph")));
            ASSERT_NE(asset, nullptr);
            ASSERT_TRUE(CompileGraphDefinition(graph, asset->Data().graph));
            // Clip/rig are supplied by fake providers; packaging dependencies are tested by the builder suite.
            ProductGroupTransaction transaction;
            ASSERT_TRUE(manager->BeginProductGroup(graphId, bundle, std::array<Uuid, 1>{graphId}, transaction));
            ProductGroupSnapshotPtr published;
            ASSERT_TRUE(manager->SaveProductGroup(transaction, bundle, std::array<AssetPtr, 1>{asset}, published));
        }
        Actor *AddGraph(float value = 1.f)
        {
            auto *actor       = Add();
            auto *component   = actor->GetComponent<AnimationComponent>();
            auto  config      = component->GetData();
            config.mode       = AnimationMode::Graph;
            config.graph      = graphId;
            config.loop       = false;
            config.parameters = {{parameter, false, false, value}};
            component->SetConfig(config);
            return actor;
        }
        void Bind()
        {
            world->Tick(0.f);
            AssetExecutor::Get()->WaitForAll();
            world->Tick(0.f);
        }
        Uuid                  graphId   = Uuid::Create();
        Uuid                  parameter = Uuid::Create();
        GraphDefinition       graph;
        std::filesystem::path directory;
        NativeFileSystemPtr   products;
        std::string           bundle;
    };
} // namespace

TEST_F(AnimationGraphWorldTest, ProductBindingParametersAndControlArePerInstance)
{
    auto *a = AddGraph();
    auto *b = AddGraph(0.f);
    Bind();
    const auto first = host->Find(world.Get(), a->GetUuid());
    ASSERT_EQ(first.state, AnimationSourceState::Ready) << first.diagnostic.message;
    ASSERT_EQ(first.parameters.size(), 1u);
    EXPECT_FLOAT_EQ(std::get<float>(first.parameters[0].value), 1.f);
    EXPECT_EQ(first.animation.asset, graphId);
    world->StartSimulation();
    world->Tick(5.f);
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), a->GetUuid()).models[0][3].x, 4.f / 3.f);
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), b->GetUuid()).models[0][3].x, 0.f);
    const auto advanced = host->Find(world.Get(), a->GetUuid());
    EXPECT_EQ(advanced.poseRevision, first.poseRevision + 1);
    EXPECT_EQ(host->Control(first.handle, AnimationControlOp::Seek, 2.f).error, AnimationError::Unsupported);
    EXPECT_FALSE(host->SetParameter(first.handle, parameter, true));
    EXPECT_FALSE(host->SetParameter(first.handle, Uuid::Create(), 1.f));
    EXPECT_FALSE(host->SetParameter(first.handle, parameter, std::numeric_limits<float>::infinity()));
    EXPECT_FLOAT_EQ(std::get<float>(host->Find(world.Get(), a->GetUuid()).parameters[0].value), 1.f);
    EXPECT_TRUE(host->Control(first.handle, AnimationControlOp::Pause));
    EXPECT_TRUE(host->SetParameter(first.handle, parameter, 0.5f));
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), a->GetUuid()).models[0][3].x, 2.f / 3.f);
    world->Tick(1.f);
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), a->GetUuid()).models[0][3].x, 2.f / 3.f);
    EXPECT_TRUE(host->Control(first.handle, AnimationControlOp::Stop));
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), a->GetUuid()).models[0][3].x, 0.f);
    EXPECT_FLOAT_EQ(std::get<float>(host->Find(world.Get(), a->GetUuid()).parameters[0].value), 0.5f);
    std::stringstream stream;
    StreamArchive     storage(stream);
    {
        JsonOutputArchive output(storage);
        a->GetComponent<AnimationComponent>()->SaveJson(output);
    }
    AnimationComponent restored;
    JsonInputArchive   input(storage);
    restored.LoadJson(input);
    EXPECT_EQ(restored.GetData(), a->GetComponent<AnimationComponent>()->GetData());
}

TEST_F(AnimationGraphWorldTest, ReloadKeepsOldTupleUntilAllClipsValidateAndRejectsNewBinding)
{
    auto *actor = AddGraph();
    Bind();
    auto first = host->Find(world.Get(), actor->GetUuid());
    ASSERT_EQ(first.state, AnimationSourceState::Ready) << first.diagnostic.message;
    EXPECT_TRUE(host->SetParameter(first.handle, parameter, 0.25f));
    world->StartSimulation();
    world->Tick(1.f);
    const auto old         = host->Describe(world.Get(), actor->GetUuid());
    auto       candidate   = clips.Publish(clipId, Clip(8.f));
    candidate->value.state = AnimationSourceState::Pending;
    Publish();
    Bind();
    EXPECT_TRUE(host->Find(world.Get(), actor->GetUuid()).retainedVersion);
    EXPECT_EQ(host->Describe(world.Get(), actor->GetUuid()).revision, old.revision);
    candidate->value.state = AnimationSourceState::Ready;
    world->Tick(0.5f);
    const auto next = host->Find(world.Get(), actor->GetUuid());
    ASSERT_EQ(next.state, AnimationSourceState::Ready) << next.diagnostic.message;
    EXPECT_FALSE(next.retainedVersion);
    EXPECT_NE(next.animation.generation, first.animation.generation);
    EXPECT_FLOAT_EQ(std::get<float>(next.parameters[0].value), 0.25f);
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), actor->GetUuid()).models[0][3].x, 1.f / 3.f);
    graph.skeleton = Uuid::Create();
    Publish();
    Bind();
    const auto failed = host->Find(world.Get(), actor->GetUuid());
    EXPECT_EQ(failed.state, AnimationSourceState::Error);
    EXPECT_TRUE(failed.retainedVersion);
    EXPECT_EQ(failed.animation, next.animation);
    actor->GetComponent<AnimationComponent>()->SetGraph(Uuid::Create());
    EXPECT_EQ(host->Describe(world.Get(), actor->GetUuid()).state, SkeletalPoseState::Error);
    Bind();
    EXPECT_FALSE(host->Find(world.Get(), actor->GetUuid()).retainedVersion);
    EXPECT_EQ(host->SetParameter(first.handle, parameter, 1.f).error, AnimationError::StaleGeneration);
}

TEST_F(AnimationGraphWorldTest, InvalidInitialOverridesCannotPublishAnInstance)
{
    auto *actor = AddGraph();
    actor->GetComponent<AnimationComponent>()->SetParameters({{parameter, false, false, 1.f}, {parameter, false, false, 0.f}});
    Bind();
    EXPECT_EQ(host->Find(world.Get(), actor->GetUuid()).state, AnimationSourceState::Error);
    EXPECT_EQ(host->Describe(world.Get(), actor->GetUuid()).state, SkeletalPoseState::Error);
    actor->GetComponent<AnimationComponent>()->SetParameters({{parameter, true, true, 0.f}});
    Bind();
    EXPECT_EQ(host->Find(world.Get(), actor->GetUuid()).state, AnimationSourceState::Error);
}

TEST_F(AnimationWorldTest, IndependentPlayersAdvanceOnceBeforePoseQueries)
{
    auto *a = Add();
    auto *b = Add(false);
    world->StartSimulation();
    world->Tick(0.75f);
    const auto sa = host->Find(world.Get(), a->GetUuid());
    const auto sb = host->Find(world.Get(), b->GetUuid());
    ASSERT_EQ(sa.state, AnimationSourceState::Ready) << sa.diagnostic.message;
    EXPECT_FLOAT_EQ(sa.time, 0.75f);
    EXPECT_FLOAT_EQ(sb.time, 0.f);
    EXPECT_EQ(sa.poseRevision, 1u);
    EXPECT_EQ(sb.poseRevision, 1u);
    const auto pose = host->Describe(world.Get(), a->GetUuid());
    ASSERT_EQ(pose.state, SkeletalPoseState::Ready);
    ASSERT_EQ(pose.models.size(), 2u);
    EXPECT_FLOAT_EQ(pose.models[0][3].x, 1.f);
    EXPECT_FLOAT_EQ(pose.models[1][3].y, 2.f);
    EXPECT_EQ(host->Query(pose.instance, pose.skeleton).revision, pose.revision);
    EXPECT_EQ(host->Find(world.Get(), a->GetUuid()).poseRevision, 1u);
    EXPECT_TRUE(host->Control(sa.handle, AnimationControlOp::Pause));
    EXPECT_TRUE(host->Control(sb.handle, AnimationControlOp::Play));
    world->Tick(0.5f);
    EXPECT_FLOAT_EQ(host->Find(world.Get(), a->GetUuid()).time, 0.75f);
    EXPECT_FLOAT_EQ(host->Find(world.Get(), b->GetUuid()).time, 0.5f);
    EXPECT_TRUE(host->Control(sa.handle, AnimationControlOp::Seek, 2.f));
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), a->GetUuid()).models[0][3].x, 8.f / 3.f);
    EXPECT_TRUE(host->Control(sa.handle, AnimationControlOp::Stop));
    EXPECT_FLOAT_EQ(host->Find(world.Get(), a->GetUuid()).time, 0.f);
    EXPECT_FALSE(host->Control(sa.handle, AnimationControlOp::SetRate, -1.f));
    EXPECT_FLOAT_EQ(host->Find(world.Get(), a->GetUuid()).rate, 1.f);
}

TEST_F(AnimationWorldTest, EditWorldDoesNotAdvanceAndRootPolicyNeverWritesActor)
{
    auto *a = Add();
    world->Tick(1.f);
    EXPECT_FLOAT_EQ(host->Find(world.Get(), a->GetUuid()).time, 0.f);
    a->GetComponent<AnimationComponent>()->SetRootPolicy(AnimationRootPolicy::LockToReference);
    world->StartSimulation();
    world->Tick(1.f);
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), a->GetUuid()).models[0][3].x, 0.f);
    EXPECT_FLOAT_EQ(a->GetComponent<TransformComponent>()->GetLocalTransform().translation.x, 0.f);
    world->StopSimulation();
    EXPECT_EQ(host->Describe(world.Get(), a->GetUuid()).state, SkeletalPoseState::Error);
}

TEST_F(AnimationWorldTest, ReloadPublishesWholeTupleAndFailurePreservesExplicitOldVersion)
{
    auto *a = Add();
    world->StartSimulation();
    world->Tick(0.5f);
    const auto initial     = host->Find(world.Get(), a->GetUuid());
    const auto oldPose     = host->Describe(world.Get(), a->GetUuid());
    auto       candidate   = clips.Publish(clipId, Clip(8.f));
    candidate->value.state = AnimationSourceState::Pending;
    ASSERT_TRUE(host->Control(initial.handle, AnimationControlOp::Reload));
    world->Tick(1.f);
    auto pending = host->Find(world.Get(), a->GetUuid());
    EXPECT_TRUE(pending.retainedVersion);
    EXPECT_EQ(pending.animation, initial.animation);
    EXPECT_EQ(host->Describe(world.Get(), a->GetUuid()).revision, oldPose.revision);
    candidate->value.state = AnimationSourceState::Ready;
    world->Tick(0.f);
    const auto ready = host->Find(world.Get(), a->GetUuid());
    EXPECT_FALSE(ready.retainedVersion);
    EXPECT_NE(ready.animation.generation, initial.animation.generation);
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), a->GetUuid()).models[0][3].x, 4.f / 3.f);
    auto failed          = clips.Publish(clipId, Clip());
    failed->value.state  = AnimationSourceState::Error;
    failed->value.status = {AnimationError::InvalidAsset, "failed recook"};
    ASSERT_TRUE(host->Control(ready.handle, AnimationControlOp::Reload));
    world->Tick(1.f);
    const auto retained = host->Find(world.Get(), a->GetUuid());
    EXPECT_EQ(retained.state, AnimationSourceState::Error);
    EXPECT_TRUE(retained.retainedVersion);
    EXPECT_EQ(retained.animation, ready.animation);
    a->GetComponent<AnimationComponent>()->SetClip(Uuid::Create());
    EXPECT_EQ(host->Describe(world.Get(), a->GetUuid()).state, SkeletalPoseState::Error);
    world->Tick(0.f);
    EXPECT_FALSE(host->Find(world.Get(), a->GetUuid()).retainedVersion);
    EXPECT_EQ(host->Control(ready.handle, AnimationControlOp::Play).error, AnimationError::StaleGeneration);
}

TEST_F(AnimationWorldTest, PendingResultsCannotReviveRemovedOrReboundInstances)
{
    clips.records[clipId]->value.state = AnimationSourceState::Pending;
    auto      *a                       = Add();
    const auto actorId                 = a->GetUuid();
    world->Tick(0.f);
    const auto initial = host->Find(world.Get(), actorId);
    EXPECT_EQ(initial.state, AnimationSourceState::Pending);
    a->RemoveComponent<AnimationComponent>();
    EXPECT_EQ(host->Control(initial.handle, AnimationControlOp::Play).error, AnimationError::StaleGeneration);
    clips.records[clipId]->value.state = AnimationSourceState::Ready;
    world->Tick(0.f);
    EXPECT_EQ(host->Describe(world.Get(), actorId).state, SkeletalPoseState::Error);
    auto *b = Add();
    world->Tick(0.f);
    const auto owned  = host->Describe(world.Get(), b->GetUuid());
    const auto handle = host->Find(world.Get(), b->GetUuid()).handle;
    world->Reset();
    EXPECT_EQ(host->Control(handle, AnimationControlOp::Play).error, AnimationError::StaleGeneration);
    EXPECT_EQ(owned.models.size(), 2u);
    EXPECT_EQ(host->Query(owned.instance, owned.skeleton).state, SkeletalPoseState::Error);
}

TEST_F(AnimationWorldTest, RigMismatchProviderAbsenceAndDrainAreExplicit)
{
    auto *a                                   = Add();
    clips.records[clipId]->value.skeletonHint = Uuid::Create();
    world->Tick(0.f);
    EXPECT_EQ(host->Find(world.Get(), a->GetUuid()).diagnostic.error, AnimationError::InvalidAsset);
    Interface<IAnimationClipSource>::Get()->UnRegister();
    auto *b = Add();
    world->Tick(0.f);
    EXPECT_EQ(host->Find(world.Get(), b->GetUuid()).diagnostic.error, AnimationError::MissingProvider);
    Interface<IAnimationClipSource>::Get()->Register(clips);
    clips.records[clipId]->value.skeletonHint = {};
    auto *c                                   = Add();
    world->Tick(0.f);
    auto pose        = host->Describe(world.Get(), c->GetUuid());
    auto wrong       = pose.skeleton;
    wrong.generation = Uuid::Create();
    EXPECT_EQ(host->Query(pose.instance, wrong).state, SkeletalPoseState::Error);
    host->Drain();
    world->Tick(1.f);
    EXPECT_EQ(host->Describe(world.Get(), c->GetUuid()).state, SkeletalPoseState::Error);
    EXPECT_EQ(pose.models.size(), 2u);
}

TEST_F(AnimationWorldTest, ProductChangeRequestsNewSnapshotAndSerializedConfigOmitsRuntime)
{
    auto *a = Add();
    world->StartSimulation();
    world->Tick(0.75f);
    const auto before = host->Find(world.Get(), a->GetUuid());
    clips.Publish(clipId, Clip(8.f));
    ++clips.serial;
    world->Tick(0.f);
    const auto after = host->Find(world.Get(), a->GetUuid());
    EXPECT_NE(before.animation.generation, after.animation.generation);
    EXPECT_FLOAT_EQ(host->Describe(world.Get(), a->GetUuid()).models[0][3].x, 2.f);
    std::stringstream stream;
    StreamArchive     storage(stream);
    {
        JsonOutputArchive archive(storage);
        a->GetComponent<AnimationComponent>()->SaveJson(archive);
    }
    EXPECT_EQ(stream.str().find("poseRevision"), std::string::npos);
    EXPECT_EQ(stream.str().find("generation"), std::string::npos);
    AnimationComponent restored;
    JsonInputArchive   input(storage);
    restored.LoadJson(input);
    EXPECT_EQ(restored.GetData(), a->GetComponent<AnimationComponent>()->GetData());
    EXPECT_FALSE(restored.GetAttachment());
}

TEST_F(AnimationWorldTest, WorldRetainsModuleThroughVirtualSubsystemDestructor)
{
    struct Probe final : IWorldSubSystem {
        Probe(std::weak_ptr<const void> code, bool &observed) : lifetime(std::move(code)), alive(observed)
        {
        }
        ~Probe() override
        {
            alive = !lifetime.expired();
        }
        std::weak_ptr<const void> lifetime;
        bool                     &alive;
    };
    auto                      code                  = std::make_shared<int>(2);
    std::weak_ptr<const void> lifetime              = code;
    bool                      aliveDuringDestructor = false;
    world->AddSubSystem(Name("ModuleLeaseProbe"), new Probe(code, aliveDuringDestructor), code);
    code.reset();
    EXPECT_FALSE(lifetime.expired());
    world.Reset(nullptr);
    EXPECT_TRUE(aliveDuringDestructor);
    EXPECT_TRUE(lifetime.expired());
}
