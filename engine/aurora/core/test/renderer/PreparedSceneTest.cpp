//
// Created on 2026/10/16.
//

#include "prepare/PreparedScene.h"

#include "resource/ResourceCatalog.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <limits>
#include <thread>

using namespace sky;
using namespace sky::aurora;

namespace {
    constexpr SceneId    a{0, 1}, b{1, 1};
    constexpr ViewportId output{0, 1};
    RenderResourceKey    Resource(uint32_t product, uint64_t revision = 1)
    {
        return {Uuid::CreateWithSeed(product), revision, 1, 81};
    }
    RenderEntityDesc Entity(float x = 0, float z = 0.5f)
    {
        RenderEntityDesc desc;
        desc.world.v[12] = x;
        desc.world.v[14] = z;
        desc.mesh        = {Resource(1), 1};
        desc.material    = {Resource(2), 1};
        return desc;
    }
    ViewDesc View(SceneId scene = a, float x = 0)
    {
        ViewDesc view;
        view.scene            = scene;
        view.output           = output;
        view.view.v[12]       = -x;
        view.cameraPosition.x = x;
        return view;
    }
    class PreparedSceneTest : public testing::Test {
    protected:
        void SetUp() override
        {
            catalog = std::move(std::get<std::unique_ptr<ResourceCatalog>>(ResourceCatalog::Create({81})));
            Apply({CreateScene{a, {}}, CreateScene{b, {}}, CreateViewport{output, {}}, CreateView{{0, 1}, View()}, CreateView{{1, 1}, View(a, 4)},
                   CreateView{{2, 1}, View(b)}});
        }
        void Apply(std::vector<RenderCommand> commands)
        {
            FramePacket packet;
            packet.sequence = ++sequence;
            packet.commands = std::move(commands);
            auto result     = store.Apply(packet);
            ASSERT_TRUE(result.Succeeded());
            for (auto &retired : result.retirements)
                ASSERT_TRUE(store.AcknowledgeRetirement(retired));
        }
        ResourceTicket Request(RenderEntityRef entity, ResourceBindingSlot slot, uint64_t bindingRevision = 1)
        {
            auto r = catalog->Request(store, {entity, slot, bindingRevision, sequence});
            EXPECT_TRUE(std::holds_alternative<ResourceRequest>(r));
            return std::holds_alternative<ResourceRequest>(r) ? std::get<ResourceRequest>(r).ticket : ResourceTicket{};
        }
        void Ready(const ResourceTicket &ticket, std::optional<AABB> bounds = AABB{Vector3(-0.1f), Vector3(0.1f)}, std::vector<float> lods = {2})
        {
            const auto status = std::get<ResourceCatalogStatus>(catalog->GetStatus(ticket));
            if (status.state == RenderResourceState::RESIDENT)
                return;
            RenderResourcePayload payload{ticket.resource, Name("prepare-test"), std::vector<uint8_t>(16), {bounds, std::move(lods)}};
            ASSERT_FALSE(catalog->Supply(ticket, payload));
            ASSERT_FALSE(catalog->AdoptResident(ticket, ticket.resource, std::make_shared<ResourceBacking>()));
        }
        void Bind(RenderEntityRef entity)
        {
            Ready(Request(entity, ResourceBindingSlot::MESH));
            Ready(Request(entity, ResourceBindingSlot::MATERIAL), {}, {});
        }
        std::shared_ptr<const PreparedFrame> Run(uint32_t workers = 3, size_t chunk = 2, bool serial = false, bool shuffled = false)
        {
            auto resources = catalog->Freeze(store);
            auto frame     = store.Freeze();
            EXPECT_TRUE(frame);
            auto built = BuildPreparedSceneProgram(*frame, resources.cpu, chunk);
            if (auto *error = std::get_if<RendererError>(&built)) {
                ADD_FAILURE() << error->message;
                return {};
            }
            auto program = std::move(std::get<PreparedSceneProgram>(built));
            EXPECT_EQ(program.work.systems.size(), frame->Scenes().size() + frame->Views().size());
            ScenePrepareInit info;
            info.workers          = workers;
            info.maxParallelNodes = serial ? 0 : 256;
            auto created          = ScenePrepareScheduler::Create(info);
            auto kernel           = std::move(program.work.kernel);
            if (shuffled) {
                kernel = [original = std::move(kernel)](const FrozenPrepareContext &context) {
                    if (context.Chunk()->index == 0)
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    original(context);
                };
            }
            auto submitted =
                created.scheduler->Submit(*frame, program.work.systems, std::move(kernel), program.work.partitions, program.work.outputs);
            if (!submitted.Accepted()) {
                ADD_FAILURE() << submitted.error->message;
                return {};
            }
            EXPECT_EQ(submitted.group->UsedSerialFallback(), serial);
            auto result = submitted.group->Wait();
            if (!result.Succeeded()) {
                ADD_FAILURE() << result.error;
                for (const auto &node : result.nodes)
                    ADD_FAILURE() << node.diagnostic;
                return {};
            }
            if (shuffled && result.products) {
                auto products = std::make_shared<PrepareProducts>(*result.products);
                std::reverse(products->records.begin(), products->records.end());
                result.products = std::move(products);
            }
            auto collected = CollectPreparedScene(program, result);
            EXPECT_TRUE(std::holds_alternative<std::shared_ptr<const PreparedFrame>>(collected));
            return std::holds_alternative<std::shared_ptr<const PreparedFrame>>(collected) ? std::get<std::shared_ptr<const PreparedFrame>>(collected)
                                                                                           : nullptr;
        }
        uint64_t PreparedSequence(RenderEntityRef id)
        {
            const auto *scene = store.FindScene(id.scene);
            return scene->registry.Get<RenderObjectData>(scene->FindEntity(id.entity)->entity)->preparedSequence;
        }
        CpuSceneStore                    store;
        std::unique_ptr<ResourceCatalog> catalog;
        uint64_t                         sequence = 0;
    };
} // namespace

TEST_F(PreparedSceneTest, SharedSceneTwoCamerasAndAnotherSceneAreIsolated)
{
    auto lit             = Entity();
    lit.properties.light = {{0.5f, 1, 0}, 3, 0.25f};
    Apply({CreateRenderEntity{{a, {0, 1}}, lit}, CreateRenderEntity{{a, {1, 1}}, Entity(4)}, CreateRenderEntity{{b, {0, 1}}, lit}});
    Bind({a, {0, 1}});
    Bind({a, {1, 1}});
    Bind({b, {0, 1}});
    auto frame = Run();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->views.size(), 3u);
    for (const auto &view : frame->views)
        ASSERT_EQ(view.draws.size(), 1u);
    EXPECT_EQ(frame->views[0].draws[0].entity, (RenderEntityRef{a, {0, 1}}));
    EXPECT_EQ(frame->views[1].draws[0].entity, (RenderEntityRef{a, {1, 1}}));
    EXPECT_EQ(frame->views[2].draws[0].entity.scene, b);
    ASSERT_EQ(frame->views[0].lights.size(), 1u);
    EXPECT_FLOAT_EQ(frame->views[0].lights[0].position.z, 0.5f);
    EXPECT_TRUE(frame->views[1].lights.empty());
    EXPECT_EQ(frame->views[2].lights.size(), 1u);
    EXPECT_EQ(PreparedSequence({a, {0, 1}}), sequence);
}

TEST_F(PreparedSceneTest, ViewOnlyChangeReusesObjectsButSelectsLodAndTags)
{
    auto other            = Entity();
    other.properties.tags = 2;
    Apply({CreateRenderEntity{{a, {0, 1}}, Entity()}, CreateRenderEntity{{a, {1, 1}}, other}});
    Bind({a, {0, 1}});
    Bind({a, {1, 1}});
    ASSERT_TRUE(Run());
    const auto prepared   = sequence;
    auto       view       = View();
    view.cameraPosition.z = 5;
    view.drawTags         = 2;
    Apply({UpdateView{{0, 1}, view, 1}});
    auto frame = Run();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->views[0].draws.size(), 1u);
    EXPECT_EQ(frame->views[0].draws[0].entity.entity.index, 1u);
    EXPECT_EQ(frame->views[0].draws[0].lod, 1u);
    EXPECT_EQ(PreparedSequence({a, {0, 1}}), prepared);
    EXPECT_EQ(frame->views[0].sceneRevision, prepared);
}

TEST_F(PreparedSceneTest, ShadowCastersSurviveCameraCullingAndKeepOwnedFlags)
{
    auto hidden                      = Entity(4);
    hidden.properties.castShadow     = true;
    hidden.properties.receiveShadow  = true;
    hidden.properties.subMesh        = 3;
    auto visible                     = Entity();
    visible.properties.castShadow    = true;
    visible.properties.receiveShadow = false;
    auto masked                      = hidden;
    masked.visibilityMask            = 0;
    Apply({CreateRenderEntity{{a, {0, 1}}, hidden}, CreateRenderEntity{{a, {1, 1}}, visible}, CreateRenderEntity{{a, {2, 1}}, masked}});
    for (uint32_t i = 0; i < 3; ++i)
        Bind({a, {i, 1}});
    const auto serial   = Run(1, 1, true);
    const auto parallel = Run(3, 2, false, true);
    ASSERT_TRUE(serial && parallel);
    const auto &view = parallel->views[0];
    ASSERT_EQ(view.draws.size(), 1u);
    EXPECT_FALSE(view.draws[0].receiveShadow);
    ASSERT_EQ(view.shadowCasters.size(), 2u);
    ASSERT_EQ(serial->views[0].shadowCasters.size(), view.shadowCasters.size());
    for (size_t i = 0; i < view.shadowCasters.size(); ++i)
        EXPECT_EQ(view.shadowCasters[i].entity, serial->views[0].shadowCasters[i].entity);
    const auto caster =
        std::find_if(view.shadowCasters.begin(), view.shadowCasters.end(), [](const auto &draw) { return draw.entity.entity.index == 0; });
    ASSERT_NE(caster, view.shadowCasters.end());
    EXPECT_EQ(caster->subMesh, 3u);
    EXPECT_TRUE(caster->receiveShadow);
    EXPECT_FLOAT_EQ(caster->world.v[12], 4);
    EXPECT_EQ(caster->mesh.resource, Resource(1));
}

TEST_F(PreparedSceneTest, SerialParallelAndShuffledChunksHaveIdenticalStablePackets)
{
    std::vector<RenderCommand> commands;
    for (uint32_t i = 0; i != 61; ++i) {
        auto desc            = Entity(i % 2 ? 4.0f : 0.0f);
        desc.properties.tags = 1 + i % 3;
        if (i % 3 == 0)
            desc.properties.light = {{.25f, .5f, 1}, float(i + 1), .5f};
        commands.emplace_back(CreateRenderEntity{{a, {60 - i, 1}}, desc});
    }
    Apply(std::move(commands));
    for (uint32_t i = 0; i != 61; ++i)
        Bind({a, {i, 1}});
    const auto serial   = Run(1, 7, true);
    const auto parallel = Run(4, 5, false, true);
    ASSERT_TRUE(serial && parallel);
    for (size_t v = 0; v != serial->views.size(); ++v) {
        const auto &expected = serial->views[v].draws, &actual = parallel->views[v].draws;
        ASSERT_EQ(actual.size(), expected.size());
        for (size_t i = 0; i != expected.size(); ++i) {
            EXPECT_EQ(actual[i].entity, expected[i].entity);
            EXPECT_EQ(actual[i].mesh.resource, expected[i].mesh.resource);
            EXPECT_EQ(actual[i].lod, expected[i].lod);
            EXPECT_EQ(actual[i].tags, expected[i].tags);
            for (size_t j = 0; j != 16; ++j)
                EXPECT_NEAR(actual[i].world.v[j], expected[i].world.v[j], 1e-5f);
            EXPECT_NEAR(actual[i].worldBounds->min.x, expected[i].worldBounds->min.x, 1e-5f);
        }
        const auto &expectedLights = serial->views[v].lights, &actualLights = parallel->views[v].lights;
        ASSERT_EQ(actualLights.size(), expectedLights.size());
        for (size_t i = 0; i != expectedLights.size(); ++i) {
            EXPECT_EQ(actualLights[i].entity, expectedLights[i].entity);
            EXPECT_NEAR(actualLights[i].position.x, expectedLights[i].position.x, 1e-5f);
            EXPECT_NEAR(actualLights[i].position.z, expectedLights[i].position.z, 1e-5f);
            EXPECT_FLOAT_EQ(actualLights[i].light.color.y, expectedLights[i].light.color.y);
            EXPECT_FLOAT_EQ(actualLights[i].light.intensity, expectedLights[i].light.intensity);
            EXPECT_FLOAT_EQ(actualLights[i].light.range, expectedLights[i].light.range);
        }
    }
}

TEST_F(PreparedSceneTest, ResidentVersionChangeInvalidatesBoundsWithoutEntityUpdate)
{
    Apply({CreateRenderEntity{{a, {0, 1}}, Entity()}});
    Bind({a, {0, 1}});
    auto old = Run();
    ASSERT_TRUE(old);
    auto desc = Entity();
    desc.mesh = {Resource(1, 2), 2};
    Apply({UpdateRenderEntity{{a, {0, 1}}, desc, 1}});
    auto ticket   = Request({a, {0, 1}}, ResourceBindingSlot::MESH, 2);
    auto fallback = Run();
    ASSERT_TRUE(fallback);
    ASSERT_EQ(fallback->views[0].draws.size(), 1u);
    EXPECT_EQ(fallback->views[0].draws[0].mesh.resource.revision, 1u);
    const auto lastPrepared = sequence;
    Ready(ticket, AABB{{3.9f, -0.1f, -0.1f}, {4.1f, 0.1f, 0.1f}}, {});
    Apply({});
    auto latest = Run();
    ASSERT_TRUE(latest);
    EXPECT_TRUE(latest->views[0].draws.empty());
    ASSERT_EQ(latest->views[1].draws.size(), 1u);
    EXPECT_EQ(latest->views[1].draws[0].mesh.resource.revision, 2u);
    EXPECT_EQ(latest->views[1].draws[0].lod, 0u);
    EXPECT_GT(PreparedSequence({a, {0, 1}}), lastPrepared);
    EXPECT_NEAR(old->views[0].draws[0].worldBounds->min.x, -0.1f, 1e-5f);
    EXPECT_EQ(old->views[0].draws[0].mesh.resource.revision, 1u);
}

TEST_F(PreparedSceneTest, DeletionAndGenerationReplacementDoNotReuseOldBindings)
{
    Apply({CreateRenderEntity{{a, {0, 1}}, Entity()}});
    Bind({a, {0, 1}});
    auto old = Run();
    ASSERT_TRUE(old);
    Apply({DestroyRenderEntity{{a, {0, 1}}}});
    Apply({CreateRenderEntity{{a, {0, 2}}, Entity()}});
    auto empty = Run();
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->views[0].draws.empty());
    Bind({a, {0, 2}});
    auto latest = Run();
    ASSERT_TRUE(latest);
    ASSERT_EQ(latest->views[0].draws.size(), 1u);
    EXPECT_EQ(latest->views[0].draws[0].entity.entity.generation, 2u);
    EXPECT_EQ(old->views[0].draws[0].entity.entity.generation, 1u);
}

TEST_F(PreparedSceneTest, MissingResourcesSkipDrawButStillPrepareLight)
{
    auto desc             = Entity();
    desc.properties.light = {{1, 1, 1}, 1, 1};
    Apply({CreateRenderEntity{{a, {0, 1}}, desc}});
    auto frame = Run();
    ASSERT_TRUE(frame);
    EXPECT_TRUE(frame->views[0].draws.empty());
    EXPECT_EQ(frame->views[0].lights.size(), 1u);
    Ready(Request({a, {0, 1}}, ResourceBindingSlot::MESH));
    frame = Run();
    ASSERT_TRUE(frame);
    EXPECT_TRUE(frame->views[0].draws.empty());
}

TEST_F(PreparedSceneTest, SixClipPlanesRejectOutsideAndKeepTouchingBounds)
{
    std::vector<RenderCommand> commands;
    const Vector3 positions[] = {{-2, 0, .5f}, {2, 0, .5f}, {0, -2, .5f}, {0, 2, .5f}, {0, 0, -1}, {0, 0, 2}, {1.1f, 0, .5f}, {0, 0, .5f}};
    for (uint32_t i = 0; i != std::size(positions); ++i) {
        auto desc        = Entity(positions[i].x, positions[i].z);
        desc.world.v[13] = positions[i].y;
        commands.emplace_back(CreateRenderEntity{{a, {i, 1}}, desc});
    }
    Apply(std::move(commands));
    for (uint32_t i = 0; i != std::size(positions); ++i)
        Bind({a, {i, 1}});
    auto frame = Run();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->views[0].draws.size(), 2u);
    EXPECT_EQ(frame->views[0].draws[0].entity.entity.index, 6u);
    EXPECT_EQ(frame->views[0].draws[1].entity.entity.index, 7u);
    auto reversed             = View();
    reversed.projection.v[10] = -1;
    reversed.projection.v[14] = 1;
    Apply({UpdateView{{0, 1}, reversed, 1}});
    frame = Run();
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->views[0].draws.size(), 2u);
    EXPECT_EQ(frame->views[0].draws[0].entity.entity.index, 6u);
    EXPECT_EQ(frame->views[0].draws[1].entity.entity.index, 7u);
}

TEST_F(PreparedSceneTest, InvalidSnapshotAndMetadataFailExplicitly)
{
    Apply({CreateRenderEntity{{a, {0, 1}}, Entity()}});
    Bind({a, {0, 1}});
    auto frozen    = catalog->Freeze(store);
    auto frame     = store.Freeze();
    auto duplicate = std::make_shared<std::vector<ResolvedResourceBinding>>(*frozen.cpu);
    duplicate->push_back(duplicate->front());
    EXPECT_TRUE(std::holds_alternative<RendererError>(BuildPreparedSceneProgram(*frame, duplicate)));
    EXPECT_TRUE(std::holds_alternative<RendererError>(BuildPreparedSceneProgram(*frame, frozen.cpu, 0)));
    frame.reset();
    auto desc = Entity();
    desc.mesh = {Resource(1, 2), 2};
    Apply({UpdateRenderEntity{{a, {0, 1}}, desc, 1}});
    auto                  ticket = Request({a, {0, 1}}, ResourceBindingSlot::MESH, 2);
    RenderResourcePayload bad{ticket.resource, Name("prepare-test"), std::vector<uint8_t>(16), {{}, {4, 2}}};
    EXPECT_TRUE(catalog->Supply(ticket, bad));
}

TEST_F(PreparedSceneTest, StaleSnapshotFailsAndDoesNotPublishDrawProducts)
{
    Apply({CreateRenderEntity{{a, {0, 1}}, Entity()}});
    Bind({a, {0, 1}});
    auto frozen = catalog->Freeze(store);
    auto desc   = Entity();
    desc.mesh   = {Resource(1, 2), 2};
    Apply({UpdateRenderEntity{{a, {0, 1}}, desc, 1}});
    auto frame     = store.Freeze();
    auto program   = std::get<PreparedSceneProgram>(BuildPreparedSceneProgram(*frame, frozen.cpu));
    auto scheduler = ScenePrepareScheduler::Create();
    auto submitted = scheduler.scheduler->Submit(*frame, program.work.systems, program.work.kernel, program.work.partitions, program.work.outputs);
    ASSERT_TRUE(submitted.Accepted());
    const auto &result = submitted.group->Wait();
    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.products);
    EXPECT_TRUE(std::holds_alternative<RendererError>(CollectPreparedScene(program, result)));
}

TEST_F(PreparedSceneTest, TwoSceneProducersActuallyOverlapBeforeViewsReadTheirResults)
{
    Apply({CreateRenderEntity{{a, {0, 1}}, Entity()}, CreateRenderEntity{{b, {0, 1}}, Entity()}});
    Bind({a, {0, 1}});
    Bind({b, {0, 1}});
    auto                    resources = catalog->Freeze(store);
    auto                    frame     = store.Freeze();
    auto                    program   = std::get<PreparedSceneProgram>(BuildPreparedSceneProgram(*frame, resources.cpu, 1));
    auto                    scheduler = ScenePrepareScheduler::Create({2});
    std::mutex              mutex;
    std::condition_variable gate;
    uint32_t                entered    = 0;
    std::atomic_bool        overlapped = true;
    auto kernel = [original = std::move(program.work.kernel), &mutex, &gate, &entered, &overlapped](const FrozenPrepareContext &context) {
        if (context.Id() <= 2) {
            std::unique_lock lock(mutex);
            ++entered;
            gate.notify_all();
            if (!gate.wait_for(lock, std::chrono::seconds(2), [&] { return entered == 2; }))
                overlapped = false;
        }
        original(context);
    };
    auto submitted = scheduler.scheduler->Submit(*frame, program.work.systems, std::move(kernel), program.work.partitions, program.work.outputs);
    ASSERT_TRUE(submitted.Accepted());
    const auto &result = submitted.group->Wait();
    ASSERT_TRUE(result.Succeeded());
    EXPECT_TRUE(overlapped);
    EXPECT_EQ(entered, 2u);
    auto prepared = std::get<std::shared_ptr<const PreparedFrame>>(CollectPreparedScene(program, result));
    frame.reset();
    ASSERT_FALSE(store.IsFrozen());
    Apply({DestroyScene{a}, DestroyScene{b}});
    ASSERT_EQ(prepared->views[0].draws.size(), 1u);
    ASSERT_EQ(prepared->views[2].draws.size(), 1u);
}

TEST_F(PreparedSceneTest, AffineScaleAndShearUseConservativeWorldBounds)
{
    auto desc       = Entity();
    desc.world.v[0] = -2;
    desc.world.v[4] = .5f;
    desc.world.v[5] = 3;
    Apply({CreateRenderEntity{{a, {0, 1}}, desc}});
    Bind({a, {0, 1}});
    auto prepared = Run();
    ASSERT_TRUE(prepared);
    ASSERT_EQ(prepared->views[0].draws.size(), 1u);
    const auto &bounds = *prepared->views[0].draws[0].worldBounds;
    EXPECT_NEAR(bounds.min.x, -.25f, 1e-5f);
    EXPECT_NEAR(bounds.max.x, .25f, 1e-5f);
    EXPECT_NEAR(bounds.min.y, -.3f, 1e-5f);
    EXPECT_NEAR(bounds.max.y, .3f, 1e-5f);
    EXPECT_NEAR(bounds.min.z, .4f, 1e-5f);
    EXPECT_NEAR(bounds.max.z, .6f, 1e-5f);
}

TEST_F(PreparedSceneTest, UnknownBoundsStayVisibleAndVisibilityMasksFilterLightsAndDraws)
{
    auto desc             = Entity(20);
    desc.properties.light = {{1, 1, 1}, 1, 100};
    desc.visibilityMask   = 2;
    Apply({CreateRenderEntity{{a, {0, 1}}, desc}});
    Ready(Request({a, {0, 1}}, ResourceBindingSlot::MESH), {});
    Ready(Request({a, {0, 1}}, ResourceBindingSlot::MATERIAL), {}, {});
    auto prepared = Run();
    ASSERT_TRUE(prepared);
    ASSERT_EQ(prepared->views[0].draws.size(), 1u);
    auto view           = View();
    view.visibilityMask = 1;
    Apply({UpdateView{{0, 1}, view, 1}});
    prepared = Run();
    ASSERT_TRUE(prepared);
    EXPECT_TRUE(prepared->views[0].draws.empty());
    EXPECT_TRUE(prepared->views[0].lights.empty());
}

TEST_F(PreparedSceneTest, InvalidLightAndProjectiveWorldAreRejectedBeforeMutation)
{
    for (uint32_t fault = 0; fault != 2; ++fault) {
        auto desc = Entity();
        if (fault)
            desc.world.v[15] = 2;
        else
            desc.properties.light.intensity = std::numeric_limits<float>::quiet_NaN();
        FramePacket packet;
        packet.sequence = ++sequence;
        packet.commands = {CreateRenderEntity{{a, {0, 1}}, desc}};
        EXPECT_FALSE(store.Apply(packet).Succeeded());
        EXPECT_TRUE(store.FindScene(a)->entities.empty());
    }
}

TEST_F(PreparedSceneTest, BoundsOverflowFailsWithoutPublishingPartialSceneOrProducts)
{
    auto desc       = Entity();
    desc.world.v[0] = 4;
    Apply({CreateRenderEntity{{a, {0, 1}}, desc}});
    Ready(Request({a, {0, 1}}, ResourceBindingSlot::MESH),
          AABB{Vector3(-std::numeric_limits<float>::max() * .5f), Vector3(std::numeric_limits<float>::max() * .5f)});
    Ready(Request({a, {0, 1}}, ResourceBindingSlot::MATERIAL), {}, {});
    auto resources = catalog->Freeze(store);
    auto frame     = store.Freeze();
    auto program   = std::get<PreparedSceneProgram>(BuildPreparedSceneProgram(*frame, resources.cpu, 1));
    auto scheduler = ScenePrepareScheduler::Create({2});
    auto submitted =
        scheduler.scheduler->Submit(*frame, program.work.systems, std::move(program.work.kernel), program.work.partitions, program.work.outputs);
    ASSERT_TRUE(submitted.Accepted());
    const auto &result = submitted.group->Wait();
    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.products);
    EXPECT_TRUE(std::holds_alternative<RendererError>(CollectPreparedScene(program, result)));
    frame.reset();
    EXPECT_FALSE(store.IsFrozen());
    EXPECT_EQ(PreparedSequence({a, {0, 1}}), 0u);
}

TEST_F(PreparedSceneTest, RepresentativeWorkloadReportsBuildDispatchWaitAndMergeCosts)
{
    constexpr uint32_t count = 8192;
    Apply({DestroyView{{2, 1}}, UpdateView{{1, 1}, View(), 1}});
    std::vector<RenderCommand> commands;
    for (uint32_t i = 0; i != count; ++i)
        commands.emplace_back(CreateRenderEntity{{a, {i, 1}}, Entity()});
    Apply(std::move(commands));
    for (uint32_t i = 0; i != count; ++i)
        Bind({a, {i, 1}});
    auto resources = catalog->Freeze(store);
    for (auto workers : {1u, 4u}) {
        auto scheduler = ScenePrepareScheduler::Create({workers});
        for (size_t chunk : {256u, 1024u}) {
            for (bool dirty : {true, false}) {
                if (dirty) {
                    std::vector<RenderCommand> updates;
                    for (uint32_t i = 0; i != count; ++i)
                        updates.emplace_back(UpdateRenderEntity{{a, {i, 1}}, Entity(), sequence + 1});
                    Apply(std::move(updates));
                }
                auto       frame   = store.Freeze();
                const auto begin   = std::chrono::steady_clock::now();
                auto       program = std::get<PreparedSceneProgram>(BuildPreparedSceneProgram(*frame, resources.cpu, chunk));
                const auto built   = std::chrono::steady_clock::now();
                auto       submitted =
                    scheduler.scheduler->Submit(*frame, program.work.systems, program.work.kernel, program.work.partitions, program.work.outputs);
                ASSERT_TRUE(submitted.Accepted());
                const auto  dispatched = std::chrono::steady_clock::now();
                const auto &result     = submitted.group->Wait();
                ASSERT_TRUE(result.Succeeded());
                const auto waited    = std::chrono::steady_clock::now();
                auto       collected = CollectPreparedScene(program, result);
                const auto merged    = std::chrono::steady_clock::now();
                ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const PreparedFrame>>(collected));
                EXPECT_EQ(std::get<std::shared_ptr<const PreparedFrame>>(collected)->views[0].draws.size(), count);
                const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
                std::printf("B08 rows=%u views=2 workers=%u chunk=%zu dirty=%d build=%.3f submit=%.3f wait=%.3f merge=%.3f ms serial=%d\n", count,
                            workers, chunk, dirty, ms(begin, built), ms(built, dispatched), ms(dispatched, waited), ms(waited, merged),
                            submitted.group->UsedSerialFallback());
            }
        }
    }
}


TEST_F(PreparedSceneTest, SkinningSnapshotsDriveBoundsAndOutliveSceneMutation)
{
    auto desc = Entity(5.f);
    auto skin = std::make_shared<RenderSkinningData>();
    skin->binding = {desc.mesh.resource.product, Uuid::Create(), Uuid::Create(), 1, 1, 1, 123, Uuid::Create(), Uuid::Create(), Uuid::Create()};
    skin->worldEpoch = Uuid::Create();
    skin->instanceGeneration = Uuid::Create();
    skin->poseRevision = 1;
    skin->matrices = {Matrix4::Identity(), Matrix4::Identity()};
    skin->matrices[0][3].x = -5.f;
    skin->bounds = {{-5.1f, -.1f, -.1f}, {-4.9f, .1f, .1f}};
    desc.properties.skinning = skin;
    desc.properties.castShadow = true;
    Apply({CreateRenderEntity{{a, {0, 1}}, desc}});
    Bind({a, {0, 1}});
    const auto first = Run();
    ASSERT_NE(first, nullptr);
    ASSERT_EQ(first->views[0].draws.size(), 1u);
    const auto &draw = first->views[0].draws[0];
    ASSERT_NE(draw.skinning, UINT32_MAX);
    ASSERT_NE(first->views[0].skinning, nullptr);
    EXPECT_EQ((*first->views[0].skinning)[draw.skinning], skin);
    EXPECT_NEAR(draw.worldBounds->min.x, -.1f, 1e-5f);
    EXPECT_EQ(first->views[0].shadowCasters[0].skinning, draw.skinning);
    auto next = std::make_shared<RenderSkinningData>(*skin);
    next->poseRevision = 2;
    next->bounds = {{-.1f, -.1f, -.1f}, {.1f, .1f, .1f}};
    next->matrices[0][3].x = 0.f;
    desc.properties.skinning = next;
    Apply({UpdateRenderEntity{{a, {0, 1}}, desc, 1}});
    const auto second = Run();
    ASSERT_NE(second, nullptr);
    EXPECT_TRUE(second->views[0].draws.empty());
    Apply({DestroyRenderEntity{{a, {0, 1}}}});
    EXPECT_FLOAT_EQ((*first->views[0].skinning)[draw.skinning]->matrices[0][3].x, -5.f);
}
