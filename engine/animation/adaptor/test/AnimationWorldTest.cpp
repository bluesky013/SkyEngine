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
#include <functional>
#include <gtest/gtest.h>
#include <limits>
#include <rapidjson/stringbuffer.h>
#include <sstream>

using namespace sky;
using namespace sky::animation;
using namespace sky::aurora;

namespace {
    class PoseContributor final : public IAnimationPoseContributor {
    public:
        AnimationPoseContribution Evaluate(const AnimationPoseBase &base) const override
        {
            if (evaluate) {
                return evaluate(base);
            }
            AnimationPoseContribution result{{}, base.binding, base.revision, 1, base.models, {}};
            result.models[1][3].x += 3.f;
            for (const auto &model : result.models) {
                result.modelNormals.emplace_back(model.InverseTranspose());
            }
            return result;
        }
        std::function<AnimationPoseContribution(const AnimationPoseBase &)> evaluate;
    };
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
            return AnimationSourceRequest<T>{std::make_shared<Operation<T>>(found->second)};
        }
        std::shared_ptr<Record<T>> Publish(const Uuid &id, CounterPtr<T> resource, uint64_t layout = 0)
        {
            auto next            = std::make_shared<Record<T>>();
            next->value.resource = std::move(resource);
            next->value.state    = AnimationSourceState::Ready;
            next->value.version  = {id, Uuid::Create(), 1, layout};
            records[id]          = next;
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
            host  = CreateAnimationWorldHost();
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
            ASSERT_TRUE(manager->SaveAsset(asset, bundle));
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

TEST_F(AnimationWorldTest, PendingPoseQueryValidatesIdentityBeforeSkeletonBecomesReady)
{
    const auto record   = rigs.records[rigId];
    record->value.state = AnimationSourceState::Pending;
    auto *actor         = Add();
    ASSERT_TRUE(actor->GetComponent<AnimationComponent>()->SetMode(AnimationMode::ReferencePose));
    world->Tick(0.f);
    const auto pending = host->Describe(world.Get(), actor->GetUuid());
    ASSERT_EQ(pending.state, SkeletalPoseState::Pending);
    ASSERT_TRUE(pending.instance.generation);
    EXPECT_FALSE(pending.skeleton.generation);
    const SkeletalRigVersion expected{rigId, record->value.version.generation, record->value.version.revision, record->value.version.layout};
    const auto               queried = host->Query(pending.instance, expected);
    EXPECT_EQ(queried.state, SkeletalPoseState::Pending);
    EXPECT_TRUE(queried.models.empty());
    EXPECT_TRUE(queried.modelNormals.empty());
    auto stale       = pending.instance;
    stale.worldEpoch = Uuid::Create();
    EXPECT_EQ(host->Query(stale, expected).state, SkeletalPoseState::Error);
    stale            = pending.instance;
    stale.generation = Uuid::Create();
    EXPECT_EQ(host->Query(stale, expected).state, SkeletalPoseState::Error);

    record->value.state = AnimationSourceState::Ready;
    world->Tick(0.f);
    const auto ready = host->Query(pending.instance, expected);
    ASSERT_EQ(ready.state, SkeletalPoseState::Ready);
    EXPECT_EQ(ready.skeleton, expected);
    EXPECT_EQ(ready.models.size(), 2u);
    auto wrong       = expected;
    wrong.generation = Uuid::Create();
    EXPECT_EQ(host->Query(pending.instance, wrong).state, SkeletalPoseState::Error);
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

TEST_F(AnimationWorldTest, WorldAndPoseRemainOwnedAfterHostShutdown)
{
    auto *actor = Add();
    world->StartSimulation();
    world->Tick(.5f);
    auto pose = host->Describe(world.Get(), actor->GetUuid());
    ASSERT_EQ(pose.state, SkeletalPoseState::Ready);
    ASSERT_EQ(pose.models.size(), 2u);
    host.reset();
    EXPECT_NE(world->GetSubSystem(Name(ANIMATION_WORLD_SYSTEM)), nullptr);
    world->Tick(.5f);
    world.Reset(nullptr);
    EXPECT_EQ(pose.models.size(), 2u);
}

TEST_F(AnimationWorldTest, ReferencePoseNeedsOnlySkeletonAndComposesOwnedModifier)
{
    Interface<IAnimationClipSource>::Get()->UnRegister();
    auto *actor     = Add();
    auto *component = actor->GetComponent<AnimationComponent>();
    ASSERT_TRUE(component->SetMode(AnimationMode::ReferencePose));
    component->SetClip({});
    world->Tick(0.f);
    const auto status = host->Find(world.Get(), actor->GetUuid());
    ASSERT_EQ(status.state, AnimationSourceState::Ready);
    EXPECT_FALSE(status.animation.asset);
    EXPECT_FALSE(status.playing);
    const auto base = host->Describe(world.Get(), actor->GetUuid());
    ASSERT_EQ(base.models.size(), 2u);
    EXPECT_TRUE(base.modelNormals.empty());
    EXPECT_FLOAT_EQ(base.models[1][3].y, 2.f);
    AnimationPoseContributionToken token;
    const auto                     contributor = std::make_shared<PoseContributor>();
    ASSERT_TRUE(host->RegisterPoseContributor({status.handle, status.skeleton}, contributor, token));
    AnimationPoseContributionToken duplicate;
    EXPECT_EQ(host->RegisterPoseContributor(token.binding, contributor, duplicate).error, AnimationError::InvalidPlayback);
    const auto modified = host->Describe(world.Get(), actor->GetUuid());
    ASSERT_EQ(modified.state, SkeletalPoseState::Ready);
    EXPECT_FLOAT_EQ(modified.models[1][3].x, 3.f);
    EXPECT_FLOAT_EQ(base.models[1][3].x, 0.f);
    EXPECT_EQ(modified.modelNormals.size(), 2u);
    EXPECT_GT(modified.revision, base.revision);
    EXPECT_EQ(host->Query(modified.instance, modified.skeleton).revision, modified.revision);
    EXPECT_EQ(host->Control(status.handle, AnimationControlOp::Play).error, AnimationError::Unsupported);
    ASSERT_TRUE(host->UnregisterPoseContributor(token));
    const auto restored = host->Describe(world.Get(), actor->GetUuid());
    EXPECT_FLOAT_EQ(restored.models[1][3].x, 0.f);
    EXPECT_TRUE(restored.modelNormals.empty());
    EXPECT_GT(restored.revision, modified.revision);
    EXPECT_EQ(host->UnregisterPoseContributor(token).error, AnimationError::StaleGeneration);
}

TEST_F(AnimationWorldTest, BadOrStaleContributionRetainsCompletePoseAndNormals)
{
    auto *actor = Add();
    ASSERT_TRUE(actor->GetComponent<AnimationComponent>()->SetMode(AnimationMode::ReferencePose));
    world->Tick(0.f);
    const auto                     status      = host->Find(world.Get(), actor->GetUuid());
    const auto                     contributor = std::make_shared<PoseContributor>();
    AnimationPoseContributionToken token;
    ASSERT_TRUE(host->RegisterPoseContributor({status.handle, status.skeleton}, contributor, token));
    const auto good = host->Describe(world.Get(), actor->GetUuid());
    ASSERT_EQ(good.state, SkeletalPoseState::Ready);
    for (uint32_t failure = 0; failure < 5; ++failure) {
        contributor->evaluate = [failure](const AnimationPoseBase &base) {
            AnimationPoseContribution value{{}, base.binding, base.revision, 2, base.models, {Matrix4::Identity(), Matrix4::Identity()}};
            if (failure == 0) {
                value.models.pop_back();
            } else if (failure == 1) {
                value.modelNormals[0][0].x = 0.f;
            } else if (failure == 2) {
                value.modelNormals[1][1].y = std::numeric_limits<float>::infinity();
            } else if (failure == 3) {
                value.binding.skeleton.generation = Uuid::Create();
            } else {
                ++value.baseRevision;
            }
            return value;
        };
        world->Tick(0.f);
        const auto retained = host->Describe(world.Get(), actor->GetUuid());
        EXPECT_EQ(retained.state, SkeletalPoseState::Ready);
        EXPECT_TRUE(retained.retainedVersion);
        EXPECT_EQ(retained.revision, good.revision);
        EXPECT_EQ(retained.modelNormals.size(), good.modelNormals.size());
        EXPECT_FLOAT_EQ(retained.models[1][3].x, good.models[1][3].x);
        EXPECT_FALSE(retained.diagnostic.empty());
    }
    contributor->evaluate = {};
    const auto repaired   = host->Describe(world.Get(), actor->GetUuid());
    EXPECT_FALSE(repaired.retainedVersion);
    EXPECT_GT(repaired.revision, good.revision);
}

TEST_F(AnimationWorldTest, ContributionRevocationAndWorldDetachInsideCallbackAreSafe)
{
    auto *actor = Add();
    world->Tick(0.f);
    const auto                           status      = host->Find(world.Get(), actor->GetUuid());
    auto                                 contributor = std::make_shared<PoseContributor>();
    const std::weak_ptr<PoseContributor> weak        = contributor;
    AnimationPoseContributionToken       token;
    contributor->evaluate = [&](const AnimationPoseBase &base) {
        EXPECT_TRUE(host->UnregisterPoseContributor(token));
        return AnimationPoseContribution{{}, base.binding, base.revision, 1, base.models, {}};
    };
    ASSERT_TRUE(host->RegisterPoseContributor({status.handle, status.skeleton}, contributor, token));
    contributor.reset();
    const auto retained = host->Describe(world.Get(), actor->GetUuid());
    EXPECT_TRUE(retained.retainedVersion);
    EXPECT_TRUE(weak.expired());
    contributor           = std::make_shared<PoseContributor>();
    contributor->evaluate = [&](const AnimationPoseBase &base) {
        world.Reset(nullptr);
        return AnimationPoseContribution{{}, base.binding, base.revision, 1, base.models, {}};
    };
    ASSERT_TRUE(host->RegisterPoseContributor({status.handle, status.skeleton}, contributor, token));
    const auto stale = host->Query(retained.instance, retained.skeleton);
    EXPECT_EQ(stale.state, SkeletalPoseState::Error);
    EXPECT_TRUE(stale.models.empty());
    EXPECT_TRUE(stale.modelNormals.empty());
    EXPECT_EQ(host->RegisterPoseContributor(token.binding, contributor, token).error, AnimationError::StaleGeneration);
}

TEST_F(AnimationWorldTest, ReferenceModeRoundTripsAndRawInvalidModeDoesNotNarrowOrOverwrite)
{
    AnimationComponent component;
    ASSERT_TRUE(component.SetMode(AnimationMode::ReferencePose));
    component.SetSkeleton(rigId);
    std::stringstream serialized;
    StreamArchive     storage(serialized);
    {
        JsonOutputArchive archive(storage);
        component.SaveJson(archive);
    }
    AnimationComponent restored;
    JsonInputArchive   input(storage);
    ASSERT_TRUE(restored.TryLoadJson(input));
    EXPECT_EQ(restored.GetData(), component.GetData());
    EXPECT_FALSE(restored.SetMode(AnimationMode::END));
    const auto before = restored.GetData();
    for (const uint64_t raw : {uint64_t(AnimationMode::END), uint64_t(AnimationMode::END) + 1, uint64_t(258), UINT64_MAX}) {
        rapidjson::Document json;
        json.Parse(serialized.str().c_str());
        json["elements"]["mode"].SetUint64(raw);
        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json.Accept(writer);
        std::stringstream invalid(buffer.GetString());
        StreamArchive     invalidStorage(invalid);
        JsonInputArchive  archive(invalidStorage);
        EXPECT_EQ(restored.TryLoadJson(archive).error, AnimationError::InvalidAsset);
        EXPECT_EQ(restored.GetData(), before);
        std::string error;
        EXPECT_FALSE(restored.TryAttachToWorld(error));
        EXPECT_FALSE(error.empty());
    }
    const auto *mode = GetTypeNode(TypeInfo<AnimationMode>::RegisteredId());
    ASSERT_NE(mode, nullptr);
    EXPECT_EQ(mode->enums.size(), 3u);
    EXPECT_FALSE(mode->enums.contains(static_cast<uint64_t>(AnimationMode::END)));
}
